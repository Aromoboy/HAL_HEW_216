#include "SweetsManager.h"

#include <sstream>
#include <utility>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif
#include <vector>
#include "AABBCollider.h"
#include "Cake.h"
#include "Core/UpdateScheduler.h"
#include "ModelCache.h"
#include "Pdding.h"
#include "Sherbet.h"
#include "Sweets.h"
#include "Systems/DrawSystem.h"
#include "Systems/PhysicsMachine.h"

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
}

#if defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

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
    constexpr float kRestVelocityThresholdSq = 0.05f * 0.05f;
    constexpr float kGroundHeightThreshold = 0.05f;

    const char* ToString(SweetsSpace::SweetsState state)
    {
        switch (state)
        {
        case SweetsSpace::SweetsState::Active:
            return "Active";
        case SweetsSpace::SweetsState::Stacked:
            return "Stacked";
        case SweetsSpace::SweetsState::Ground:
            return "Ground";
        default:
            return "Unknown";
        }
    }

    float ComputeSpeedSq(const Physics::Physical& physical)
    {
        const auto& v = physical.velocity;
        return (v.x * v.x) + (v.y * v.y) + (v.z * v.z);
    }

    SweetsSpace::SweetsState EvaluateState(SweetsSpace::BaseSweets& sweets, SweetsSpace::SweetsState currentState)
    {
        (void)currentState;

        const float speedSq = ComputeSpeedSq(sweets.GetPhysical());
        const float height = sweets.GetTransform().position.y;

        if (height <= kGroundHeightThreshold && speedSq <= kRestVelocityThresholdSq)
        {
            return SweetsSpace::SweetsState::Ground;
        }

        if (speedSq <= kRestVelocityThresholdSq)
        {
            return SweetsSpace::SweetsState::Stacked;
        }

        return SweetsSpace::SweetsState::Active;
    }
}

namespace SweetsSpace
{
    SweetsManager::SweetsManager()
        : m_drawSystem(nullptr)
        , m_physicsMachine(nullptr)
        , m_scheduler(nullptr)
        , m_warnedMissingDrawSystem(false)
        , m_warnedMissingPhysicsMachine(false)
        , m_warnedMissingScheduler(false)
    {
        m_byState.emplace(SweetsState::Active, SweetsList{});
        m_byState.emplace(SweetsState::Stacked, SweetsList{});
        m_byState.emplace(SweetsState::Ground, SweetsList{});

        Render::ModelCache& cache = Render::ModelCache::Instance();

        const auto createCollider = [](const Transform& transform, float expand, const float3& offset)
        {
            return std::make_shared<Collision::AABBCollider>(transform, false, expand, offset);
        };

        {
            const std::string kind = "Cake";
            const std::string modelPath = "Assets/Model/LowPolyNature/Models/Boxs.fbx";
            const float colliderExpand = 1.05f;
            const float3 colliderOffset{ 0.0f, 0.2f, 0.0f };
            const float weight = 1.35f;
            const float damping = 0.96f;

            auto factory = [this, &cache, kind, modelPath, colliderExpand, colliderOffset, weight, damping, createCollider](const Transform& transform)
                -> std::shared_ptr<BaseSweets>
            {
                auto model = cache.Get(modelPath);
                if (!model)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Model load failed for sweets kind '" << kind
                        << "' path='" << modelPath << "'.\n";
                    DebugTrace(oss.str());
                    return nullptr;
                }

                auto collider = createCollider(transform, colliderExpand, colliderOffset);
                const float3 velocity{ 0.0f, 0.0f, 0.0f };
                const float3 force{ 0.0f, 0.0f, 0.0f };

                return MakeSweets<Cake>(kind, model, transform, collider, velocity, force, weight, damping, Core::UpdatePhase::Physics, 0);
            };

            auto [_, inserted] = m_spawner.emplace(kind, std::move(factory));
            if (!inserted)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Spawner for kind '" << kind << "' already existed. New factory ignored.\n";
                DebugTrace(oss.str());
            }
        }

        {
            const std::string kind = "Pudding";
            const std::string modelPath = "Assets/Model/LowPolyNature/Models/pudding.fbx";
            const float colliderExpand = 0.95f;
            const float3 colliderOffset{ 0.0f, -0.05f, 0.0f };
            const float weight = 1.0f;
            const float damping = 0.98f;

            auto factory = [this, &cache, kind, modelPath, colliderExpand, colliderOffset, weight, damping, createCollider](const Transform& transform)
                -> std::shared_ptr<BaseSweets>
            {
                auto model = cache.Get(modelPath);
                if (!model)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Model load failed for sweets kind '" << kind
                        << "' path='" << modelPath << "'.\n";
                    DebugTrace(oss.str());
                    return nullptr;
                }

                auto collider = createCollider(transform, colliderExpand, colliderOffset);
                const float3 velocity{ 0.0f, 0.0f, 0.0f };
                const float3 force{ 0.0f, 0.0f, 0.0f };

                return MakeSweets<Pudding>(kind, model, transform, collider, velocity, force, weight, damping, Core::UpdatePhase::Physics, 0);
            };

            auto [_, inserted] = m_spawner.emplace(kind, std::move(factory));
            if (!inserted)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Spawner for kind '" << kind << "' already existed. New factory ignored.\n";
                DebugTrace(oss.str());
            }
        }

        {
            const std::string kind = "Sherbet";
            const std::string modelPath = "Assets/Model/LowPolyNature/Models/Ball.fbx";
            const float colliderExpand = 0.9f;
            const float3 colliderOffset{ 0.0f, 0.1f, 0.0f };
            const float weight = 0.85f;
            const float damping = 0.99f;

            auto factory = [this, &cache, kind, modelPath, colliderExpand, colliderOffset, weight, damping, createCollider](const Transform& transform)
                -> std::shared_ptr<BaseSweets>
            {
                auto model = cache.Get(modelPath);
                if (!model)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Model load failed for sweets kind '" << kind
                        << "' path='" << modelPath << "'.\n";
                    DebugTrace(oss.str());
                    return nullptr;
                }

                auto collider = createCollider(transform, colliderExpand, colliderOffset);
                const float3 velocity{ 0.0f, 0.0f, 0.0f };
                const float3 force{ 0.0f, 0.0f, 0.0f };

                return MakeSweets<Sherbet>(kind, model, transform, collider, velocity, force, weight, damping, Core::UpdatePhase::Physics, 0);
            };

            auto [_, inserted] = m_spawner.emplace(kind, std::move(factory));
            if (!inserted)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Spawner for kind '" << kind << "' already existed. New factory ignored.\n";
                DebugTrace(oss.str());
            }
        }
    }

    SweetsManager::~SweetsManager() = default;

    void SweetsManager::OnUpdate(Core::UpdateContext& ctx)
    {
        struct PendingStateChange
        {
            std::shared_ptr<BaseSweets> sweets;
            SweetsState state;
        };

        std::vector<PendingStateChange> stateChanges;

        for (auto& entry : m_byState)
        {
            SweetsState bucketState = entry.first;
            auto& sweetsList = entry.second;

            auto it = sweetsList.begin();
            while (it != sweetsList.end())
            {
                std::shared_ptr<BaseSweets> sweets = *it;

                if (!sweets)
                {
#if defined(_DEBUG)
                    std::ostringstream oss;
                    oss << kLogPrefix << "Purging null sweets entry from state "
                        << ToString(bucketState) << "\n";
                    DebugTrace(oss.str());
#endif
                    it = sweetsList.erase(it);
                    continue;
                }

                if (!sweets->IsEnabled())
                {
#if defined(_DEBUG)
                    std::ostringstream oss;
                    oss << kLogPrefix << "Purging sweets ptr=" << sweets.get()
                        << " from state " << ToString(bucketState)
                        << " because update node was disabled.\n";
                    DebugTrace(oss.str());
#endif
                    m_lookup.erase(sweets);
                    it = sweetsList.erase(it);
                    continue;
                }

                sweets->OnUpdate(ctx);

                const SweetsState desiredState = EvaluateState(*sweets, bucketState);
                if (desiredState != bucketState)
                {
                    stateChanges.push_back({ sweets, desiredState });
                }

                ++it;
            }
        }

        for (const auto& change : stateChanges)
        {
            auto lookup = m_lookup.find(change.sweets);
            const SweetsState previousState = (lookup != m_lookup.end()) ? lookup->second : SweetsState::Active;
            SetState(change.sweets, change.state);
#if defined(_DEBUG)
            std::ostringstream oss;
            oss << kLogPrefix << "State change ptr=" << change.sweets.get()
                << " " << ToString(previousState) << " -> "
                << ToString(change.state) << "\n";
            DebugTrace(oss.str());
#endif
        }
    }

    void SweetsManager::DrawAll(Render::ModelRenderer& renderer, const std::shared_ptr<CameraSpace::ICamera>& camera)
    {
        if (!camera)
        {
            return;
        }

        for (auto& entry : m_byState)
        {
            SweetsList& sweetsList = entry.second;
            for (auto& sweets : sweetsList)
            {
                if (sweets)
                {
                    sweets->Draw(renderer, camera);
                }
            }
        }
    }

    SweetsManager::SweetsList& SweetsManager::GetByState(SweetsState state)
    {
        return m_byState[state];
    }

    const SweetsManager::SweetsList& SweetsManager::GetByState(SweetsState state) const
    {
        static const SweetsList kEmpty;
        auto it = m_byState.find(state);
        if (it == m_byState.end())
        {
            return kEmpty;
        }
        return it->second;
    }

    void SweetsManager::SetState(const std::shared_ptr<BaseSweets>& sweets, SweetsState state)
    {
        if (!sweets)
        {
            return;
        }

        auto found = m_lookup.find(sweets);
        if (found != m_lookup.end())
        {
            if (found->second == state)
            {
                return;
            }

            auto& currentList = GetByState(found->second);
            currentList.remove(sweets);
        }

        auto& targetList = GetByState(state);
        targetList.push_back(sweets);
        m_lookup[sweets] = state;
    }

    void SweetsManager::Erase(const std::shared_ptr<BaseSweets>& sweets)
    {
        if (!sweets)
        {
            return;
        }

        auto found = m_lookup.find(sweets);
        if (found == m_lookup.end())
        {
            return;
        }

        auto& currentList = GetByState(found->second);
        currentList.remove(sweets);
        m_lookup.erase(found);
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

    void SweetsManager::SetPhysicsMachine(Systems::PhysicsMachine* physicsMachine)
    {
        if (m_physicsMachine == physicsMachine)
        {
            return;
        }

        m_physicsMachine = physicsMachine;
        m_warnedMissingPhysicsMachine = false;
    }

    void SweetsManager::SetUpdateScheduler(Core::UpdateScheduler* scheduler)
    {
        if (m_scheduler == scheduler)
        {
            return;
        }

        m_scheduler = scheduler;
        m_warnedMissingScheduler = false;
    }

    std::shared_ptr<BaseSweets> SweetsManager::Spawn(const std::string& kind, const Transform& transform)
    {
        auto it = m_spawner.find(kind);
        if (it == m_spawner.end())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawn requested for unknown kind '" << kind << "'. Registered kinds:";
            if (m_spawner.empty())
            {
                oss << " (none)";
            }
            else
            {
                bool first = true;
                for (const auto& entry : m_spawner)
                {
                    oss << (first ? ' ' : ',') << entry.first;
                    first = false;
                }
            }
            oss << "\n";
            DebugTrace(oss.str());
            return nullptr;
        }

        auto sweets = it->second(transform);
        if (!sweets)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawner for kind '" << kind << "' returned null instance.\n";
            DebugTrace(oss.str());
            return nullptr;
        }

        SetState(sweets, SweetsState::Active);
        RegisterWithSystems(sweets);

        return sweets;
    }

    void SweetsManager::RegisterWithSystems(const std::shared_ptr<BaseSweets>& sweets)
    {
        if (!sweets)
        {
            return;
        }

        if (m_scheduler)
        {
            m_scheduler->Register(*sweets);
        }
        else if (!m_warnedMissingScheduler)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawned sweets '" << sweets->GetTag()
                << "' missing scheduler registration: scheduler not configured.\n";
            DebugTrace(oss.str());
            m_warnedMissingScheduler = true;
        }

        if (m_physicsMachine)
        {
            m_physicsMachine->RegisterBody(*sweets);
        }
        else if (!m_warnedMissingPhysicsMachine)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawned sweets '" << sweets->GetTag()
                << "' missing physics registration: physics machine not configured.\n";
            DebugTrace(oss.str());
            m_warnedMissingPhysicsMachine = true;
        }

        if (m_drawSystem)
        {
            m_drawSystem->RegisterRenderable(*sweets);
        }
        else if (!m_warnedMissingDrawSystem)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawned sweets '" << sweets->GetTag()
                << "' missing draw registration: draw system not configured.\n";
            DebugTrace(oss.str());
            m_warnedMissingDrawSystem = true;
        }
    }
}
