#include "SweetsManager.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "AABBCollider.h"
#include "Cake.h"
#include "ModelCache.h"
#include "Pdding.h"
#include "Sherbet.h"
#include "Systems/DrawSystem.h"

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
        void DebugTrace(const std::string& message)
        {
                ::OutputDebugStringA(message.c_str());
        }
#else
        void DebugTrace(const std::string& message)
        {
                (void)message;
        }
#endif

        constexpr const char* kLogPrefix = "[SweetsManager] ";
        constexpr float kGroundHeightThreshold = 0.15f;
        constexpr float kStackedVelocityThresholdSq = 0.04f; // (0.2 m/s)^2
        constexpr const char* kFallbackModelPath = "Assets/Model/LowPolyNature/Models/OiiaioooooiaiFin.fbx";
}

SweetsManager::SweetsManager()
        : Core::UpdateNode(Core::UpdatePhase::Gameplay, 0)
        , m_drawSystem(nullptr)
        , m_registerNode()
        , m_unregisterNode()
        , m_registerPhysicsBody()
        , m_unregisterPhysicsBody()
        , m_warnedMissingScheduler(false)
        , m_warnedMissingDrawSystem(false)
        , m_warnedMissingPhysicsRegister(false)
        , m_warnedMissingPhysicsUnregister(false)
{
        InitializeBuckets();
        ConfigureRecipes();
        RegisterDefaultSpawners();
}

SweetsManager::~SweetsManager()
{
        for (auto& [state, bucket] : m_byState)
        {
                for (auto& sweet : bucket)
                {
                        UnregisterFromSystems(sweet);
                        if (sweet)
                        {
                                DebugLogDespawn(sweet.get(), state, "manager shutdown");
                        }
                }
                bucket.clear();
        }
}

void SweetsManager::InitializeBuckets()
{
        m_byState.clear();
        m_byState.emplace(SweetsState::Active, Bucket{});
        m_byState.emplace(SweetsState::Staked, Bucket{});
        m_byState.emplace(SweetsState::Ground, Bucket{});
}

void SweetsManager::ConfigureRecipes()
{
        m_recipes.clear();
        m_recipes.emplace("Cake", SweetsRecipe{ "Cake", kFallbackModelPath, 1.15f, float3{ 0.0f, 0.0f, 0.0f }, 1.8f, 0.96f });
        m_recipes.emplace("Pudding", SweetsRecipe{ "Pudding", kFallbackModelPath, 0.95f, float3{ 0.0f, 0.0f, 0.0f }, 1.2f, 0.98f });
        m_recipes.emplace("Sherbet", SweetsRecipe{ "Sherbet", kFallbackModelPath, 0.85f, float3{ 0.0f, 0.0f, 0.0f }, 0.9f, 0.99f });
}

void SweetsManager::RegisterDefaultSpawners()
{
        RegisterSpawnerForKind<SweetsSpace::Cake>("Cake");
        RegisterSpawnerForKind<SweetsSpace::Pudding>("Pudding");
        RegisterSpawnerForKind<SweetsSpace::Sherbet>("Sherbet");
}

void SweetsManager::SetDrawSystem(Systems::DrawSystem* drawSystem)
{
        if (m_drawSystem == drawSystem)
        {
                return;
        }

        m_drawSystem = drawSystem;
        m_warnedMissingDrawSystem = false;
}

void SweetsManager::SetNodeRegistrationCallbacks(RegisterNodeCallback registerFn, RegisterNodeCallback unregisterFn)
{
        m_registerNode = std::move(registerFn);
        m_unregisterNode = std::move(unregisterFn);
        m_warnedMissingScheduler = false;
}

void SweetsManager::SetPhysicsCallbacks(PhysicsRegisterCallback registerFn, PhysicsUnregisterCallback unregisterFn)
{
        m_registerPhysicsBody = std::move(registerFn);
        m_unregisterPhysicsBody = std::move(unregisterFn);
        m_warnedMissingPhysicsRegister = false;
        m_warnedMissingPhysicsUnregister = false;
}

void SweetsManager::OnUpdate(Core::UpdateContext& ctx)
{
        for (auto& [state, bucket] : m_byState)
        {
                if (bucket.empty())
                {
                        continue;
                }

                EraseUnregistered(bucket, state);

                auto it = bucket.begin();
                while (it != bucket.end())
                {
                        auto sweet = *it;
                        if (!sweet)
                        {
                                DebugLogDespawn(nullptr, state, "expired shared_ptr");
                                it = bucket.erase(it);
                                continue;
                        }

                        sweet->OnUpdate(ctx);

                        const SweetsState nextState = EvaluateState(*sweet);
                        if (nextState != state)
                        {
                                MoveSweetToState(sweet, state, nextState);
                                it = bucket.erase(it);
                        }
                        else
                        {
                                ++it;
                        }
                }
        }
}

void SweetsManager::DrawAll(ModelRender& render, const std::shared_ptr<CameraSpace::ICamera>& camera)
{
        if (!camera)
        {
                DebugTrace(std::string(kLogPrefix) + "DrawAll skipped: camera missing.\n");
                return;
        }

        for (const auto& [state, bucket] : m_byState)
        {
                (void)state;
                for (const auto& sweet : bucket)
                {
                        if (!sweet)
                        {
                                continue;
                        }

                        sweet->Draw(render, camera);
                }
        }
}

std::list<std::shared_ptr<BaseSweets>> SweetsManager::GetByState(SweetsState state)
{
        auto bucketIt = m_byState.find(state);
        if (bucketIt == m_byState.end())
        {
                return {};
        }

        Bucket& bucket = bucketIt->second;
        EraseUnregistered(bucket, state);
        return bucket;
}

std::shared_ptr<BaseSweets> SweetsManager::Spawn(const std::string& kind, const Transform& transform)
{
        auto factoryIt = m_spawners.find(kind);
        if (factoryIt == m_spawners.end())
        {
                std::ostringstream oss;
                oss << kLogPrefix << "Spawn requested for unknown kind '" << kind << "'.\n";
                DebugTrace(oss.str());
                return nullptr;
        }

        auto sweet = factoryIt->second(transform);
        if (!sweet)
        {
                std::ostringstream oss;
                oss << kLogPrefix << "Failed to instantiate sweet kind='" << kind << "'.\n";
                DebugTrace(oss.str());
                return nullptr;
        }

        auto& activeBucket = m_byState[SweetsState::Active];
        activeBucket.push_back(sweet);
        RegisterWithSystems(sweet);
        DebugLogSpawn(kind, sweet);

        return sweet;
}

void SweetsManager::RegisterWithSystems(const std::shared_ptr<BaseSweets>& sweet)
{
        if (!sweet)
        {
                return;
        }

        if (m_registerNode)
        {
                m_registerNode(*sweet);
        }
        else if (!m_warnedMissingScheduler)
        {
                DebugTrace(std::string(kLogPrefix) + "RegisterWithSystems missing scheduler callback.\n");
                m_warnedMissingScheduler = true;
        }

        if (m_drawSystem)
        {
                m_drawSystem->RegisterRenderable(*sweet);
        }
        else if (!m_warnedMissingDrawSystem)
        {
                DebugTrace(std::string(kLogPrefix) + "RegisterWithSystems missing draw system reference.\n");
                m_warnedMissingDrawSystem = true;
        }

        if (m_registerPhysicsBody)
        {
                m_registerPhysicsBody(*sweet);
        }
        else if (!m_warnedMissingPhysicsRegister)
        {
                DebugTrace(std::string(kLogPrefix) + "RegisterWithSystems missing physics callback.\n");
                m_warnedMissingPhysicsRegister = true;
        }
}

void SweetsManager::UnregisterFromSystems(const std::shared_ptr<BaseSweets>& sweet)
{
        if (!sweet)
        {
                return;
        }

        if (m_drawSystem)
        {
                m_drawSystem->UnregisterRenderable(*sweet);
        }

        if (m_unregisterNode)
        {
                m_unregisterNode(*sweet);
        }
        else if (!m_warnedMissingScheduler)
        {
                DebugTrace(std::string(kLogPrefix) + "UnregisterFromSystems missing scheduler callback.\n");
                m_warnedMissingScheduler = true;
        }

        if (m_unregisterPhysicsBody)
        {
                m_unregisterPhysicsBody(sweet.get());
        }
        else if (!m_warnedMissingPhysicsUnregister)
        {
                DebugTrace(std::string(kLogPrefix) + "UnregisterFromSystems missing physics removal callback.\n");
                m_warnedMissingPhysicsUnregister = true;
        }
}

SweetsState SweetsManager::EvaluateState(BaseSweets& sweet) const
{
        const Transform& transform = sweet.GetTransform();
        if (transform.position.y <= kGroundHeightThreshold)
        {
                return SweetsState::Ground;
        }

        const Physics::Physical& physical = sweet.GetPhysical();
        const float speedSq = physical.velocity.x * physical.velocity.x
                + physical.velocity.y * physical.velocity.y
                + physical.velocity.z * physical.velocity.z;

        if (speedSq <= kStackedVelocityThresholdSq)
        {
                return SweetsState::Staked;
        }

        return SweetsState::Active;
}

void SweetsManager::MoveSweetToState(const std::shared_ptr<BaseSweets>& sweet, SweetsState from, SweetsState to)
{
        if (!sweet)
        {
                return;
        }

        auto& targetBucket = m_byState[to];
        targetBucket.push_back(sweet);

        std::ostringstream oss;
        const auto& transform = sweet->GetTransform();
        const Physics::Physical& physical = sweet->GetPhysical();
        const float speedSq = physical.velocity.x * physical.velocity.x
                + physical.velocity.y * physical.velocity.y
                + physical.velocity.z * physical.velocity.z;
        oss << kLogPrefix << "State change tag=" << sweet->GetTag()
                << " " << StateToString(from) << " -> " << StateToString(to)
                << " posY=" << transform.position.y
                << " speedSq=" << speedSq << "\n";
        DebugTrace(oss.str());
}

bool SweetsManager::EraseUnregistered(Bucket& bucket, SweetsState state)
{
        bool removed = false;
        auto it = bucket.begin();
        while (it != bucket.end())
        {
                auto sweet = *it;
                if (!sweet || !sweet->IsEnabled())
                {
                        DebugLogDespawn(sweet.get(), state, sweet ? "disabled" : "expired shared_ptr");
                        UnregisterFromSystems(sweet);
                        it = bucket.erase(it);
                        removed = true;
                }
                else
                {
                        ++it;
                }
        }
        return removed;
}

const char* SweetsManager::StateToString(SweetsState state) const
{
        switch (state)
        {
        case SweetsState::Active: return "Active";
        case SweetsState::Staked: return "Staked";
        case SweetsState::Ground: return "Ground";
        default: return "Unknown";
        }
}

void SweetsManager::DebugLogSpawn(const std::string& kind, const std::shared_ptr<BaseSweets>& sweet) const
{
        if (!sweet)
        {
                return;
        }

        const Transform& transform = sweet->GetTransform();
        std::ostringstream oss;
        oss << kLogPrefix << "Spawned kind='" << kind << "' tag=" << sweet->GetTag()
                << " pos=(" << transform.position.x << ',' << transform.position.y << ',' << transform.position.z << ")"
                << " scale=(" << transform.scale.x << ',' << transform.scale.y << ',' << transform.scale.z << ")"
                << "\n";
        DebugTrace(oss.str());
}

void SweetsManager::DebugLogDespawn(const BaseSweets* sweet, SweetsState state, const std::string& reason) const
{
        std::ostringstream oss;
        oss << kLogPrefix << "Despawning tag=" << (sweet ? sweet->GetTag() : std::string("<null>"))
                << " state=" << StateToString(state)
                << " reason=" << reason << "\n";
        DebugTrace(oss.str());
}

template<typename TSweets>
void SweetsManager::RegisterSpawnerForKind(const std::string& kind)
{
        auto recipeIt = m_recipes.find(kind);
        if (recipeIt == m_recipes.end())
        {
                std::ostringstream oss;
                oss << kLogPrefix << "No recipe configured for kind '" << kind << "'; skipping spawner registration.\n";
                DebugTrace(oss.str());
                return;
        }

        const SweetsRecipe recipe = recipeIt->second;
        m_spawners[kind] = [this, recipe](const Transform& transform)
        {
                return InstantiateFromRecipe<TSweets>(recipe, transform);
        };
}

template<typename TSweets>
std::shared_ptr<BaseSweets> SweetsManager::InstantiateFromRecipe(const SweetsRecipe& recipe, const Transform& transform)
{
        auto model = Render::ModelCache::Instance().Get(recipe.modelPath);
        if (!model)
        {
                std::ostringstream oss;
                oss << kLogPrefix << "Model load failed for type='" << recipe.type
                        << "' path='" << recipe.modelPath << "'.\n";
                DebugTrace(oss.str());
                return nullptr;
        }

        auto collider = std::make_shared<Collision::AABBCollider>(transform, false, recipe.colliderExpand, recipe.colliderOffset);

        const std::string tag = BuildTag(recipe.type);
        auto sweet = std::make_shared<TSweets>(tag, model,
                transform.position, transform.rotation, transform.scale,
                collider,
                recipe.initialVelocity,
                recipe.initialForce,
                recipe.weight,
                recipe.damping,
                Core::UpdatePhase::Physics,
                0);
        return sweet;
}

std::string SweetsManager::BuildTag(const std::string& baseKind)
{
        size_t& counter = m_spawnCounts[baseKind];
        std::ostringstream oss;
        oss << baseKind << '_' << std::setw(4) << std::setfill('0') << counter++;
        return oss.str();
}
