#include "SweetsManager.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <random>
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
#include "SweetsPrototypeManifest.h"
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
    constexpr float kRestVelocityThresholdSq = 0.05f * 0.05f;
    constexpr float kGroundHeightThreshold = 0.05f;
    constexpr float kDefaultSpawnIntervalSeconds = 2.5f;
    constexpr int kDefaultSimultaneousSpawnCount = 1;
    constexpr float kMinimumSpawnIntervalSeconds = 0.1f;
    constexpr int kPrototypeBatchSize = 5;
    constexpr std::size_t kMaxSpawnQueueEntries = 16;
    constexpr float kDefaultSpawnHeightOffset = 8.0f;
    constexpr float kPlatformWidth = 12.0f;
    constexpr float kHorizontalOutOfBoundsMargin = 2.0f;
    constexpr float kHorizontalPlayAreaLimit = (kPlatformWidth * 0.5f) + kHorizontalOutOfBoundsMargin;

    void UpdateBatchUsage(SweetsSpace::DeleteBatchContext* batch, SweetsSpace::UseOutcome outcome)
    {
        if (!batch)
        {
            return;
        }

        if (batch->usesRemaining > 0)
        {
            --batch->usesRemaining;
        }

        if (outcome == SweetsSpace::UseOutcome::Hit)
        {
            ++batch->usesRemaining;
        }
    }

    float ComputeSpeedSq(const Physics::Physical& physical)
    {
        const auto& v = physical.velocity;
        return (v.x * v.x) + (v.y * v.y) + (v.z * v.z);
    }

    SweetsSpace::SweetsState EvaluateState(SweetsSpace::BaseSweets& sweets, SweetsSpace::SweetsState currentState)
    {
        if (currentState == SweetsSpace::SweetsState::Carried)
        {
            return SweetsSpace::SweetsState::Carried;
        }

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

    bool IsHorizontallyOutOfBounds(const SweetsSpace::BaseSweets& sweets)
    {
        float minX = 0.0f;
        float maxX = 0.0f;
        const Collision::ICollider* collider = sweets.GetCollider();
        if (collider)
        {
            const AABB bounds = collider->GetAABB();
            minX = bounds.min.x;
            maxX = bounds.max.x;
        }
        else
        {
            const Transform& transform = sweets.GetTransform();
            const float halfWidth = std::max(0.0f, transform.scale.x * 0.5f);
            minX = transform.position.x - halfWidth;
            maxX = transform.position.x + halfWidth;
        }

        const float furthest = std::max(std::abs(minX), std::abs(maxX));
        return furthest > kHorizontalPlayAreaLimit;
    }

    std::string TrimToken(const std::string& value)
    {
        auto begin = value.begin();
        auto end = value.end();
        while (begin != end && std::isspace(static_cast<unsigned char>(*begin)))
        {
            ++begin;
        }

        while (end != begin)
        {
            auto prev = end;
            --prev;
            if (!std::isspace(static_cast<unsigned char>(*prev)))
            {
                break;
            }
            end = prev;
        }

        return std::string(begin, end);
    }

    std::vector<SweetsSpace::WeightedSweetsType> ParseWeights(const std::string& raw, bool* outError)
    {
        std::vector<SweetsSpace::WeightedSweetsType> weights;
        std::string token;
        std::istringstream stream(raw);

        while (std::getline(stream, token, ','))
        {
            token = TrimToken(token);
            if (token.empty())
            {
                continue;
            }

            float weight = 1.0f;
            std::string kind = token;
            const auto colon = token.find(':');
            if (colon != std::string::npos)
            {
                kind = TrimToken(token.substr(0, colon));
                std::string weightToken = TrimToken(token.substr(colon + 1));
                if (!weightToken.empty())
                {
                    try
                    {
                        weight = std::stof(weightToken);
                    }
                    catch (const std::exception& ex)
                    {
                        if (outError)
                        {
                            *outError = true;
                        }

                        std::ostringstream oss;
                        oss << kLogPrefix << "Failed to parse weight token '" << weightToken
                            << "' from '" << raw << "': " << ex.what() << "\n";
                        DebugTrace(oss.str());
                        return {};
                    }
                }
            }

            if (!(weight > 0.0f))
            {
                continue;
            }

            const auto resolved = SweetsSpace::ParseSweetsType(kind);
            if (!resolved)
            {
                if (outError)
                {
                    *outError = true;
                }

                std::ostringstream oss;
                oss << kLogPrefix << "Unknown sweets type '" << kind
                    << "' in weight definition '" << raw << "'.\n";
                DebugTrace(oss.str());
                continue;
            }

            weights.push_back({ SweetsSpace::ToString(*resolved), weight });
        }

        if (weights.empty() && outError)
        {
            *outError = true;
        }

        return weights;
    }
}

namespace SweetsSpace
{
    const char* ToString(DeleteReason reason)
    {
        switch (reason)
        {
        case DeleteReason::Hit:
            return "Hit";
        case DeleteReason::Miss:
            return "Miss";
        default:
            return "<unknown>";
        }
    }

    const char* ToString(UseOutcome outcome)
    {
        switch (outcome)
        {
        case UseOutcome::Hit:
            return "Hit";
        case UseOutcome::Miss:
            return "Miss";
        default:
            return "<unknown>";
        }
    }

    const char* StateToString(SweetsState state)
    {
        switch (state)
        {
        case SweetsState::Active:
            return "Active";
        case SweetsState::Stacked:
            return "Stacked";
        case SweetsState::Ground:
            return "Ground";
        case SweetsState::Carried:
            return "Carried";
        default:
            return "Unknown";
        }
    }

    SweetsManager::SweetsManager()
        : m_drawSystem(nullptr)
        , m_physicsMachine(nullptr)
        , m_scheduler(nullptr)
        , m_phaseChangeCallback()
        , m_currentPhase(0)
        , m_phaseConfigs()
        , m_activePhaseConfigIndex(-1)
        , m_spawnTimer(kDefaultSpawnIntervalSeconds)
        , m_spawnQueue()
        , m_spawnOrigins()
        , m_nextSpawnOriginIndex(0)
        , m_spawnHeightOffset(kDefaultSpawnHeightOffset)
        , m_batchSize(kPrototypeBatchSize)
        , m_currentBatchRemaining(0)
        , m_random(std::random_device{}())
        , m_enableRandomRotation(false)
        , m_randomRotationMinDegrees{ 0.0f, 0.0f, 0.0f }
        , m_randomRotationMaxDegrees{ 0.0f, 360.0f, 0.0f }
        , m_enableRandomScale(true)
        , m_warnedMissingDrawSystem(false)
        , m_warnedMissingPhysicsMachine(false)
        , m_warnedMissingScheduler(false)
        , m_warnedMissingPhaseCallback(false)
        , m_warnedMissingSpawnOrigins(false)
        , m_warnedMissingHeightCollider(false)
        , m_warnedMissingBoundsCollider(false)
    {
        m_byState.emplace(SweetsState::Active, SweetsList{});
        m_byState.emplace(SweetsState::Stacked, SweetsList{});
        m_byState.emplace(SweetsState::Ground, SweetsList{});
        m_byState.emplace(SweetsState::Carried, SweetsList{});

        Render::ModelCache& cache = Render::ModelCache::Instance();
        const auto& manifest = SweetsPrototypeManifest::Instance();

        const auto createCollider = [](const Transform& transform, float3 expand, const float3& offset)
        {
            return std::make_shared<Collision::AABBCollider>(transform, false, expand, offset);
        };

        struct SweetsFactoryConfig
        {
            SweetsType type;
            float3 colliderExpand;
            float3 colliderOffset;
            float weight;
            float damping;
            float3 minScale;
            float3 maxScale;
        };

        const std::array<SweetsFactoryConfig, 3> kConfigs = { {
            { SweetsType::Cake, float3{1.05f,1.05f,1.05f} , float3{ 0.0f, 0.2f, 0.0f }, 1.35f, 0.96f,
                float3{0.85f, 0.85f, 0.85f}, float3{1.35f, 1.35f, 1.35f} },
            { SweetsType::Pudding, float3{0.95f,0.95f,0.95f}, float3{ 0.0f, -0.05f, 0.0f }, 1.0f, 0.98f,
                float3{0.9f, 0.9f, 0.9f}, float3{1.2f, 1.2f, 1.2f} },
            { SweetsType::Sherbet, float3{0.9f,0.9f,0.9f}, float3{ 0.0f, 0.1f, 0.0f }, 0.85f, 0.99f,
                float3{0.75f, 0.75f, 0.75f}, float3{1.4f, 1.4f, 1.4f} },
        } };

        for (const auto& config : kConfigs)
        {
            const auto* descriptor = manifest.Find(config.type);
            if (!descriptor)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Manifest missing prototype for sweets type '"
                    << SweetsSpace::ToString(config.type) << "'. Skipping spawner registration.\n";
                DebugTrace(oss.str());
                continue;
            }

            const std::string kind = descriptor->key;
            const std::string modelPath = descriptor->modelPath;

            std::ostringstream oss;
            oss << kLogPrefix << "Registering sweets kind '" << kind
                << "' with asset '" << modelPath << "'.\n";
            DebugTrace(oss.str());

            const SweetsType type = config.type;

            auto factory = [this, &cache, kind, modelPath, type, colliderExpand = config.colliderExpand,
                               colliderOffset = config.colliderOffset, weight = config.weight,
                               damping = config.damping, createCollider, minScale = config.minScale,
                               maxScale = config.maxScale](const Transform& transform)
                -> std::shared_ptr<BaseSweets>
            {
                auto model = cache.Get(modelPath);
                if (!model)
                {
                    std::ostringstream fail;
                    fail << kLogPrefix << "Model load failed for sweets kind '" << kind
                        << "' path='" << modelPath << "'.\n";
                    DebugTrace(fail.str());
                    return nullptr;
                }

                Transform scaledTransform = transform;
                const auto sampleScale = [&](float minValue, float maxValue)
                {
                    if (!m_enableRandomScale)
                    {
                        return 1.0f;
                    }

                    const float lo = std::min(minValue, maxValue);
                    const float hi = std::max(minValue, maxValue);
                    if (lo == hi)
                    {
                        return lo;
                    }

                    std::uniform_real_distribution<float> dist(lo, hi);
                    return dist(m_random);
                };

                const float3 sampledScale{
                    sampleScale(minScale.x, maxScale.x),
                    sampleScale(minScale.y, maxScale.y),
                    sampleScale(minScale.z, maxScale.z)
                };

                scaledTransform.scale.x *= sampledScale.x;
                scaledTransform.scale.y *= sampledScale.y;
                scaledTransform.scale.z *= sampledScale.z;

                auto collider = createCollider(scaledTransform, colliderExpand, colliderOffset);
                const float3 velocity{ 0.0f, 0.0f, 0.0f };
                const float3 force{ 0.0f, 0.0f, 0.0f };
                const float scaleFactor = sampledScale.x * sampledScale.y * sampledScale.z;
                const float scaledWeight = weight * scaleFactor;
                const float scaledDamping = std::min(damping * scaleFactor, 0.999f);

                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Factory spawn kind='" << kind << "' scale=("
                        << sampledScale.x << ',' << sampledScale.y << ',' << sampledScale.z << ")"
                        << " enabledRandomScale=" << std::boolalpha << m_enableRandomScale
                        << " weight=" << scaledWeight << " (base=" << weight << ")\n";
                    DebugTrace(oss.str());
                }

                switch (type)
                {
                case SweetsType::Cake:
                    return MakeSweets<Cake>(kind, model, scaledTransform, collider, velocity, force, scaledWeight, scaledDamping,
                        Core::UpdatePhase::Physics, 0);
                case SweetsType::Pudding:
                    return MakeSweets<Pudding>(kind, model, scaledTransform, collider, velocity, force, scaledWeight, scaledDamping,
                        Core::UpdatePhase::Physics, 0);
                case SweetsType::Sherbet:
                    return MakeSweets<Sherbet>(kind, model, scaledTransform, collider, velocity, force, scaledWeight, scaledDamping,
                        Core::UpdatePhase::Physics, 0);
                default:
                    break;
                }

                return nullptr;
            };

            auto [_, inserted] = m_spawner.emplace(kind, std::move(factory));
            if (!inserted)
            {
                std::ostringstream warn;
                warn << kLogPrefix << "Spawner for kind '" << kind
                    << "' already existed. New factory ignored.\n";
                DebugTrace(warn.str());
            }
        }

        ApplyDefaultPhaseConfigs();
    }

    SweetsManager::~SweetsManager() = default;

    void SweetsManager::OnUpdate(Core::UpdateContext& ctx)
    {
        RunSpawnScheduler(ctx.dt);

        struct PendingStateChange
        {
            std::shared_ptr<BaseSweets> sweets;
            SweetsState state;
        };

        std::vector<PendingStateChange> stateChanges;
        std::vector<std::shared_ptr<BaseSweets>> missedSweets;

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
                        << StateToString(bucketState) << "\n";
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
                        << " from state " << StateToString(bucketState)
                        << " because update node was disabled.\n";
                    DebugTrace(oss.str());
#endif
                    m_lookup.erase(sweets);
                    it = sweetsList.erase(it);
                    continue;
                }

                const bool isCarried = (bucketState == SweetsState::Carried);

                if (!isCarried)
                {
                    const bool outOfBounds = IsHorizontallyOutOfBounds(*sweets);
                    if (!sweets->GetCollider() && !m_warnedMissingBoundsCollider)
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Horizontal bounds check using transform scale for sweets ptr="
                            << sweets.get() << " (missing collider).\n";
                        DebugTrace(oss.str());
                        m_warnedMissingBoundsCollider = true;
                    }

                    if (outOfBounds)
                    {
                        missedSweets.push_back(sweets);
                        ++it;
                        continue;
                    }

                    sweets->OnUpdate(ctx);

                    const SweetsState desiredState = EvaluateState(*sweets, bucketState);
                    if (desiredState != bucketState)
                    {
                        stateChanges.push_back({ sweets, desiredState });
                    }
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
                << " " << StateToString(previousState) << " -> "
                << StateToString(change.state) << "\n";
            DebugTrace(oss.str());
#endif
        }

        DeleteBatchContext missBatch{};
        missBatch.usesRemaining = static_cast<int>(missedSweets.size());
        for (const auto& sweets : missedSweets)
        {
            RemoveSweetsEntry(sweets, DeleteReason::Miss, "MissDespawn", true, &missBatch);
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

    float SweetsManager::GetHighestStackedHeight() const
    {
        float maxHeight = 0.0f;
        const auto& stacked = GetByState(SweetsState::Stacked);
        for (const auto& sweets : stacked)
        {
            if (!sweets)
            {
                continue;
            }

            float sweetsTop = sweets->GetTransform().position.y;
            const Collision::ICollider* collider = sweets->GetCollider();
            if (collider)
            {
                const AABB bounds = collider->GetAABB();
                sweetsTop = bounds.max.y;
            }
            else
            {
                const Transform& transform = sweets->GetTransform();
                const float halfHeight = std::max(0.0f, transform.scale.y * 0.5f);
                sweetsTop = transform.position.y + halfHeight;
#if defined(_DEBUG)
                if (!m_warnedMissingHeightCollider)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "GetHighestStackedHeight fallback to transform scale"
                        << " for sweets ptr=" << sweets.get() << " (missing collider).\n";
                    DebugTrace(oss.str());
                    m_warnedMissingHeightCollider = true;
                }
#endif
            }

            maxHeight = std::max(maxHeight, sweetsTop);
        }

        return maxHeight;
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

    void SweetsManager::ClearAllSweets()
    {
        ForceSpawnRefresh();

        std::vector<std::shared_ptr<BaseSweets>> sweetsToRemove;
        sweetsToRemove.reserve(m_lookup.size());
        size_t totalStatesCleared = 0;

        for (auto& entry : m_byState)
        {
            SweetsList& list = entry.second;
            totalStatesCleared += list.size();
            for (auto& sweets : list)
            {
                if (sweets)
                {
                    sweetsToRemove.push_back(sweets);
                }
            }
            list.clear();
        }

        m_lookup.clear();

        for (const auto& sweets : sweetsToRemove)
        {
            if (!sweets)
            {
                continue;
            }

            UnregisterSweetsFromSystems(sweets, "ClearAllSweets");
        }

        std::ostringstream oss;
        oss << kLogPrefix << "ClearAllSweets removed " << sweetsToRemove.size()
            << " sweets across " << m_byState.size() << " states (" << totalStatesCleared
            << " tracked entries).\n";
        DebugTrace(oss.str());
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

    void SweetsManager::SetPhaseChangeCallback(PhaseChangeCallback callback)
    {
        m_phaseChangeCallback = std::move(callback);
        m_warnedMissingPhaseCallback = false;
    }

    void SweetsManager::NotifyPhaseChanged(int newPhase)
    {
        if (newPhase < 0)
        {
            newPhase = 0;
        }

        if (newPhase == m_currentPhase)
        {
            return;
        }

        m_currentPhase = newPhase;

        std::ostringstream oss;
        oss << kLogPrefix << "Phase changed to " << m_currentPhase << "\n";
        DebugTrace(oss.str());

        OnPhaseConfigurationChanged(m_currentPhase);

        if (m_phaseChangeCallback)
        {
            m_phaseChangeCallback(m_currentPhase);
        }
        else if (!m_warnedMissingPhaseCallback)
        {
            DebugTrace(std::string(kLogPrefix) + "Phase change callback missing; spawn layout unchanged.\n");
            m_warnedMissingPhaseCallback = true;
        }
    }

    int SweetsManager::GetCurrentPhase() const noexcept
    {
        return m_currentPhase;
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

    std::shared_ptr<BaseSweets> SweetsManager::FindNearestCarryable(const float3& position, float radius) const
    {
        const float radiusSq = radius * radius;
        std::shared_ptr<BaseSweets> best;
        float bestDistanceSq = radiusSq;

        const auto scanState = [&](SweetsState state)
        {
            auto it = m_byState.find(state);
            if (it == m_byState.end())
            {
                return;
            }

            for (const auto& sweets : it->second)
            {
                if (!sweets)
                {
                    continue;
                }

                const auto& sweetsPos = sweets->GetTransform().position;
                const float dx = sweetsPos.x - position.x;
                const float dy = sweetsPos.y - position.y;
                const float dz = sweetsPos.z - position.z;
                const float distanceSq = (dx * dx) + (dy * dy) + (dz * dz);
                if (distanceSq <= bestDistanceSq)
                {
                    bestDistanceSq = distanceSq;
                    best = sweets;
                }
            }
        };

        scanState(SweetsState::Ground);
        scanState(SweetsState::Active);

        return best;
    }

    bool SweetsManager::TryMarkAsCarried(const std::shared_ptr<BaseSweets>& sweets)
    {
        if (!sweets)
        {
            return false;
        }

        auto it = m_lookup.find(sweets);
        const SweetsState current = (it != m_lookup.end()) ? it->second : SweetsState::Active;
        if (current != SweetsState::Active && current != SweetsState::Ground)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "TryMarkAsCarried rejected tag='" << sweets->GetTag()
                << "' currentState=" << StateToString(current) << "\n";
            DebugTrace(oss.str());
            return false;
        }

        if (m_physicsMachine)
        {
            m_physicsMachine->UnregisterBody(*sweets);
        }
        else if (!m_warnedMissingPhysicsMachine)
        {
            DebugTrace(std::string(kLogPrefix) + "TryMarkAsCarried missing physics machine; body may keep simulating.\n");
            m_warnedMissingPhysicsMachine = true;
        }

        SetState(sweets, SweetsState::Carried);

        std::ostringstream oss;
        oss << kLogPrefix << "TryMarkAsCarried success tag='" << sweets->GetTag() << "'\n";
        DebugTrace(oss.str());
        return true;
    }

    bool SweetsManager::ReleaseCarriedSweets(const std::shared_ptr<BaseSweets>& sweets)
    {
        if (!sweets)
        {
            return false;
        }

        auto it = m_lookup.find(sweets);
        if (it == m_lookup.end() || it->second != SweetsState::Carried)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "ReleaseCarriedSweets rejected tag='" << sweets->GetTag() << "' state="
                << (it == m_lookup.end() ? "<unknown>" : StateToString(it->second)) << "\n";
            DebugTrace(oss.str());
            return false;
        }

        SetState(sweets, SweetsState::Active);

        if (m_physicsMachine)
        {
            m_physicsMachine->RegisterBody(*sweets);
        }
        else if (!m_warnedMissingPhysicsMachine)
        {
            DebugTrace(std::string(kLogPrefix) + "ReleaseCarriedSweets missing physics machine; body will not simulate.\n");
            m_warnedMissingPhysicsMachine = true;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "ReleaseCarriedSweets success tag='" << sweets->GetTag() << "'\n";
        DebugTrace(oss.str());
        return true;
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

    void SweetsManager::UnregisterSweetsFromSystems(const std::shared_ptr<BaseSweets>& sweets, const char* context, bool logWarnings)
    {
        if (!sweets)
        {
            return;
        }

        if (m_scheduler)
        {
            m_scheduler->Unregister(*sweets);
        }
        else if (logWarnings && !m_warnedMissingScheduler)
        {
            std::ostringstream oss;
            oss << kLogPrefix << context << " could not unregister sweets '" << sweets->GetTag()
                << "': scheduler not configured.\n";
            DebugTrace(oss.str());
            m_warnedMissingScheduler = true;
        }

        if (m_physicsMachine)
        {
            m_physicsMachine->UnregisterBody(*sweets);
        }
        else if (logWarnings && !m_warnedMissingPhysicsMachine)
        {
            std::ostringstream oss;
            oss << kLogPrefix << context << " could not unregister physics for sweets '" << sweets->GetTag()
                << "': physics machine not configured.\n";
            DebugTrace(oss.str());
            m_warnedMissingPhysicsMachine = true;
        }

        if (m_drawSystem)
        {
            m_drawSystem->UnregisterRenderable(*sweets);
        }
        else if (logWarnings && !m_warnedMissingDrawSystem)
        {
            std::ostringstream oss;
            oss << kLogPrefix << context << " could not unregister draw entry for sweets '" << sweets->GetTag()
                << "': draw system not configured.\n";
            DebugTrace(oss.str());
            m_warnedMissingDrawSystem = true;
        }
    }

    void SweetsManager::RemoveSweetsEntry(const std::shared_ptr<BaseSweets>& sweets,
        DeleteReason reason,
        const char* context,
        bool logWarnings,
        DeleteBatchContext* batchContext)
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

        const SweetsState previousState = found->second;
        auto& currentList = GetByState(previousState);
        currentList.remove(sweets);
        m_lookup.erase(found);

        const UseOutcome outcome = (reason == DeleteReason::Hit) ? UseOutcome::Hit : UseOutcome::Miss;
        UpdateBatchUsage(batchContext, outcome);

        UnregisterSweetsFromSystems(sweets, context, logWarnings);

#if defined(_DEBUG)
        const auto& pos = sweets->GetTransform().position;
        std::ostringstream oss;
        oss << kLogPrefix << context << " removed sweets tag='" << sweets->GetTag()
            << "' state=" << StateToString(previousState)
            << " deleteReason=" << SweetsSpace::ToString(reason)
            << " outcome=" << SweetsSpace::ToString(outcome)
            << " batchUsesRemaining=" << (batchContext ? batchContext->usesRemaining : -1)
            << " pos=(" << pos.x << ',' << pos.y << ',' << pos.z << ")\n";
        DebugTrace(oss.str());
#endif
    }

    bool SweetsManager::LoadPhaseSpawnConfig(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Phase spawn config not found at '" << path << "'. Using defaults.\n";
            DebugTrace(oss.str());
            ApplyDefaultPhaseConfigs();
            return false;
        }

        struct PartialConfig
        {
            PhaseSpawnConfig config;
            bool hasInterval = false;
            bool hasSimultaneous = false;
            bool hasWeights = false;
        };

        std::map<int, PartialConfig> parsed;
        int currentPhase = -1;
        bool parseError = false;
        std::string line;
        size_t lineNumber = 0;

        while (std::getline(file, line))
        {
            ++lineNumber;
            const auto commentPos = line.find_first_of("#;");
            if (commentPos != std::string::npos)
            {
                line = line.substr(0, commentPos);
            }

            line = TrimToken(line);
            if (line.empty())
            {
                continue;
            }

            const auto delimiterPos = line.find('=');
            if (delimiterPos == std::string::npos)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Spawn config line " << lineNumber << " missing '=' : " << line << "\n";
                DebugTrace(oss.str());
                parseError = true;
                continue;
            }

            const std::string key = TrimToken(line.substr(0, delimiterPos));
            const std::string value = TrimToken(line.substr(delimiterPos + 1));

            if (key == "phase")
            {
                try
                {
                    currentPhase = std::max(0, std::stoi(value));
                    parsed[currentPhase];
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse phase index on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                    currentPhase = -1;
                }
                continue;
            }

            if (currentPhase < 0)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Encountered key '" << key << "' on line " << lineNumber
                    << " before declaring a phase index.\n";
                DebugTrace(oss.str());
                parseError = true;
                continue;
            }

            auto& partial = parsed[currentPhase];
            if (key == "interval")
            {
                try
                {
                    partial.config.spawnIntervalSeconds = std::max(kMinimumSpawnIntervalSeconds, std::stof(value));
                    partial.hasInterval = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse interval on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "simultaneous")
            {
                try
                {
                    partial.config.simultaneousSpawnCount = std::max(1, std::stoi(value));
                    partial.hasSimultaneous = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse simultaneous count on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "weights")
            {
                bool weightError = false;
                auto parsedWeights = ParseWeights(value, &weightError);
                if (weightError)
                {
                    parseError = true;
                    continue;
                }

                std::map<std::string, float> collapsed;
                for (const auto& entry : parsedWeights)
                {
                    if (m_spawner.find(entry.kind) == m_spawner.end())
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Phase " << currentPhase << " references unknown sweets kind '"
                            << entry.kind << "'.\n";
                        DebugTrace(oss.str());
                        continue;
                    }
                    collapsed[entry.kind] += entry.weight;
                }

                partial.config.weights.clear();
                for (const auto& entry : collapsed)
                {
                    if (entry.second > 0.0f)
                    {
                        partial.config.weights.push_back({ entry.first, entry.second });
                    }
                }

                if (partial.config.weights.empty())
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Phase " << currentPhase
                        << " weights resolved to zero valid entries; falling back to defaults.\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
                else
                {
                    partial.hasWeights = true;
                }
            }
            else
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Unknown spawn config key '" << key << "' on line " << lineNumber << "\n";
                DebugTrace(oss.str());
            }
        }

        m_phaseConfigs.clear();
        for (const auto& entry : parsed)
        {
            PhaseSpawnConfig config = entry.second.config;
            if (!entry.second.hasInterval)
            {
                config.spawnIntervalSeconds = kDefaultSpawnIntervalSeconds;
            }
            config.spawnIntervalSeconds = std::max(config.spawnIntervalSeconds, kMinimumSpawnIntervalSeconds);

            if (!entry.second.hasSimultaneous)
            {
                config.simultaneousSpawnCount = kDefaultSimultaneousSpawnCount;
            }
            config.simultaneousSpawnCount = std::max(1, config.simultaneousSpawnCount);

            if (!entry.second.hasWeights || config.weights.empty())
            {
                config.weights = BuildDefaultWeights();
            }

            if (config.weights.empty())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Skipping phase " << entry.first
                    << " because no valid weights were available even after defaults.\n";
                DebugTrace(oss.str());
                continue;
            }

            m_phaseConfigs.push_back(std::move(config));
        }

        if (m_phaseConfigs.empty())
        {
            DebugTrace(std::string(kLogPrefix) + "No valid phase configs parsed; applying defaults.\n");
            ApplyDefaultPhaseConfigs();
            return false;
        }

        m_activePhaseConfigIndex = -1;
        OnPhaseConfigurationChanged(m_currentPhase);

        std::ostringstream oss;
        oss << kLogPrefix << "Loaded " << m_phaseConfigs.size()
            << " spawn phases from '" << path << "' (parseError=" << std::boolalpha << parseError << ").\n";
        DebugTrace(oss.str());
        return !parseError;
    }

    void SweetsManager::SetSpawnOrigins(const std::vector<float3>& origins)
    {
        m_spawnOrigins = origins;
        m_nextSpawnOriginIndex = 0;
        m_warnedMissingSpawnOrigins = m_spawnOrigins.empty();


        std::ostringstream oss;
        oss << kLogPrefix << "Configured " << m_spawnOrigins.size()
            << " spawn origins (heightOffset=" << m_spawnHeightOffset << ").\n";
        DebugTrace(oss.str());
    }

    void SweetsManager::SetRandomRotationEnabled(bool enabled)
    {
        m_enableRandomRotation = enabled;

        std::ostringstream oss;
        oss << kLogPrefix << "Random rotation " << (enabled ? "enabled" : "disabled")
            << " (minYawPitchRollDeg=" << m_randomRotationMinDegrees.x << ',' << m_randomRotationMinDegrees.y
            << ',' << m_randomRotationMinDegrees.z << " maxYawPitchRollDeg="
            << m_randomRotationMaxDegrees.x << ',' << m_randomRotationMaxDegrees.y << ','
            << m_randomRotationMaxDegrees.z << ").\n";
        DebugTrace(oss.str());
    }

    void SweetsManager::SetRandomRotationRangeDegrees(const float3& minDegrees, const float3& maxDegrees)
    {
        m_randomRotationMinDegrees = minDegrees;
        m_randomRotationMaxDegrees = maxDegrees;

        std::ostringstream oss;
        oss << kLogPrefix << "Random rotation range updated to minYawPitchRollDeg="
            << minDegrees.x << ',' << minDegrees.y << ',' << minDegrees.z
            << " maxYawPitchRollDeg=" << maxDegrees.x << ',' << maxDegrees.y << ','
            << maxDegrees.z << " (enabled=" << std::boolalpha << m_enableRandomRotation << ").\n";
        DebugTrace(oss.str());
    }

    void SweetsManager::SetRandomScaleEnabled(bool enabled)
    {
        m_enableRandomScale = enabled;

        std::ostringstream oss;
        oss << kLogPrefix << "Random scale " << (enabled ? "enabled" : "disabled") << ".\n";
        DebugTrace(oss.str());
    }

    void SweetsManager::ForceSpawnRefresh()
    {
        m_spawnTimer = 0.0f;
        if (!m_spawnQueue.empty())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Clearing pending spawn queue (" << m_spawnQueue.size() << ").\n";
            DebugTrace(oss.str());
        }
        m_spawnQueue.clear();
        m_currentBatchRemaining = 0;
    }

    void SweetsManager::RunSpawnScheduler(float deltaSeconds)
    {
        EmitSpawnRequests();

        if (m_phaseConfigs.empty())
        {
            return;
        }

        if (m_activePhaseConfigIndex < 0 || m_activePhaseConfigIndex >= static_cast<int>(m_phaseConfigs.size()))
        {
            OnPhaseConfigurationChanged(m_currentPhase);
        }

        if (!(deltaSeconds > 0.0f))
        {
            return;
        }

        m_spawnTimer += deltaSeconds;
        const auto& config = m_phaseConfigs[std::clamp(m_activePhaseConfigIndex, 0, static_cast<int>(m_phaseConfigs.size()) - 1)];
        const float interval = std::max(config.spawnIntervalSeconds, kMinimumSpawnIntervalSeconds);

        if (m_currentBatchRemaining > 0)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawn scheduler paused; active batch has "
                << m_currentBatchRemaining << " remaining.\n";
            DebugTrace(oss.str());
            return;
        }

        while (m_spawnTimer >= interval)
        {
            m_spawnTimer -= interval;
            const int requestedCount = std::max(1, m_batchSize);
            std::ostringstream oss;
            oss << kLogPrefix << "Scheduling prototype batch spawn (size=" << requestedCount
                << "; interval=" << interval << ").\n";
            DebugTrace(oss.str());
            EnqueueSpawns(requestedCount);

            if (m_currentBatchRemaining > 0)
            {
                break;
            }
        }

        EmitSpawnRequests();
    }

    void SweetsManager::EnqueueSpawns(int count)
    {
        if (count <= 0)
        {
            return;
        }

        int batchTarget = m_currentBatchRemaining;
        if (m_currentBatchRemaining <= 0)
        {
            batchTarget = std::max(1, m_batchSize);
            m_currentBatchRemaining = batchTarget;
            std::ostringstream oss;
            oss << kLogPrefix << "Initializing spawn batch (target=" << m_currentBatchRemaining
                << ", requested=" << count << ").\n";
            DebugTrace(oss.str());
        }

        const int allowedCount = std::min(count, batchTarget);
        if (allowedCount < count)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Batch capacity limited enqueues to " << allowedCount
                << " of " << count << " requests.\n";
            DebugTrace(oss.str());
        }

        int enqueued = 0;
        for (int i = 0; i < allowedCount; ++i)
        {
            if (m_spawnQueue.size() >= kMaxSpawnQueueEntries)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Spawn queue is full (" << m_spawnQueue.size()
                    << "); dropping additional requests.\n";
                DebugTrace(oss.str());
                break;
            }

            const std::string kind = ChooseKindFromActiveConfig();
            if (kind.empty())
            {
                break;
            }

            m_spawnQueue.push_back(kind);
            ++enqueued;
        }

        if (enqueued != allowedCount)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawn enqueue shortfall: enqueued=" << enqueued
                << " expected=" << allowedCount << "; queueSize=" << m_spawnQueue.size() << ".\n";
            DebugTrace(oss.str());
        }

        m_currentBatchRemaining = enqueued;
    }

    void SweetsManager::EmitSpawnRequests()
    {
        if (m_spawnQueue.empty())
        {
            return;
        }

        std::size_t spawnedCount = 0;
        const int queuedBefore = static_cast<int>(m_spawnQueue.size());
        while (!m_spawnQueue.empty())
        {
            const std::string kind = m_spawnQueue.front();
            m_spawnQueue.pop_front();

            const float3 origin = NextSpawnOrigin();
            Transform transform = BuildSpawnTransform(origin);
            const bool spawned = static_cast<bool>(Spawn(kind, transform));
            if (spawned)
            {
                ++spawnedCount;
            }

            if (m_currentBatchRemaining > 0)
            {
                --m_currentBatchRemaining;
            }
        }

        if (m_currentBatchRemaining < 0)
        {
            m_currentBatchRemaining = 0;
        }

        if (spawnedCount > 0)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Emitted " << spawnedCount << " sweets from scheduler.\n";
            DebugTrace(oss.str());
        }

        if (queuedBefore > 0)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Batch progress: queued=" << queuedBefore
                << " remaining=" << m_currentBatchRemaining << ".\n";
            DebugTrace(oss.str());

            if (m_currentBatchRemaining <= 0)
            {
                DebugTrace(std::string(kLogPrefix) + "Batch completed; scheduler may start a new batch after cooldown.\n");
            }
        }
    }

    std::string SweetsManager::ChooseKindFromActiveConfig()
    {
        if (m_phaseConfigs.empty())
        {
            return {};
        }

        const int clampedIndex = std::clamp(m_activePhaseConfigIndex, 0, static_cast<int>(m_phaseConfigs.size()) - 1);
        const auto& config = m_phaseConfigs[clampedIndex];
        if (config.weights.empty())
        {
            return {};
        }

        float totalWeight = 0.0f;
        for (const auto& entry : config.weights)
        {
            totalWeight += entry.weight;
        }

        if (!(totalWeight > 0.0f))
        {
            return {};
        }

        std::uniform_real_distribution<float> dist(0.0f, totalWeight);
        float roll = dist(m_random);
        for (const auto& entry : config.weights)
        {
            if (roll <= entry.weight)
            {
                return entry.kind;
            }
            roll -= entry.weight;
        }

        return config.weights.back().kind;
    }

    void SweetsManager::OnPhaseConfigurationChanged(int newPhase)
    {
        if (m_phaseConfigs.empty())
        {
            return;
        }

        const int clamped = std::clamp(newPhase, 0, static_cast<int>(m_phaseConfigs.size()) - 1);
        if (m_activePhaseConfigIndex == clamped)
        {
            return;
        }

        m_activePhaseConfigIndex = clamped;
        ForceSpawnRefresh();
        const float interval = std::max(m_phaseConfigs[clamped].spawnIntervalSeconds, kMinimumSpawnIntervalSeconds);
        m_spawnTimer = std::max(m_spawnTimer, interval);

        std::ostringstream oss;
        oss << kLogPrefix << "Phase change primed next batch with interval="
            << std::fixed << std::setprecision(2) << interval << " (batchSize=" << m_batchSize << ").\n";
        DebugTrace(oss.str());
        LogActiveSpawnParameters(m_phaseConfigs[clamped], clamped);
    }

    void SweetsManager::LogActiveSpawnParameters(const PhaseSpawnConfig& config, int phaseIndex) const
    {
        std::ostringstream oss;
        oss << kLogPrefix << "[Spawn] phase=" << phaseIndex
            << " interval=" << std::fixed << std::setprecision(2) << config.spawnIntervalSeconds
            << " simultaneous=" << config.simultaneousSpawnCount
            << " weights=";

        if (config.weights.empty())
        {
            oss << "(none)";
        }
        else
        {
            bool first = true;
            for (const auto& entry : config.weights)
            {
                oss << (first ? ' ' : ',') << entry.kind
                    << ':' << std::fixed << std::setprecision(2) << entry.weight;
                first = false;
            }
        }

        oss << "\n";
        DebugTrace(oss.str());
    }

    void SweetsManager::ApplyDefaultPhaseConfigs()
    {
        m_phaseConfigs.clear();
        std::vector<WeightedSweetsType> baseWeights = BuildDefaultWeights();
        if (baseWeights.empty())
        {
            DebugTrace(std::string(kLogPrefix) + "Cannot build default spawn configs: no sweets registered.\n");
            return;
        }

        PhaseSpawnConfig phase0;
        phase0.spawnIntervalSeconds = kDefaultSpawnIntervalSeconds;
        phase0.simultaneousSpawnCount = kDefaultSimultaneousSpawnCount;
        phase0.weights = baseWeights;

        PhaseSpawnConfig phase1 = phase0;
        phase1.spawnIntervalSeconds = std::max(kMinimumSpawnIntervalSeconds, kDefaultSpawnIntervalSeconds * 0.75f);

        PhaseSpawnConfig phase2 = phase0;
        phase2.spawnIntervalSeconds = std::max(kMinimumSpawnIntervalSeconds, kDefaultSpawnIntervalSeconds * 0.5f);
        phase2.simultaneousSpawnCount = 2;

        m_phaseConfigs.push_back(phase0);
        m_phaseConfigs.push_back(phase1);
        m_phaseConfigs.push_back(phase2);

        m_activePhaseConfigIndex = -1;
        ForceSpawnRefresh();

        std::ostringstream oss;
        oss << kLogPrefix << "Applied default spawn configs (" << m_phaseConfigs.size() << " phases).\n";
        DebugTrace(oss.str());
    }

    std::vector<WeightedSweetsType> SweetsManager::BuildDefaultWeights() const
    {
        std::vector<WeightedSweetsType> weights;
        weights.reserve(m_spawner.size());
        for (const auto& entry : m_spawner)
        {
            weights.push_back({ entry.first, 1.0f });
        }
        return weights;
    }

    float3 SweetsManager::NextSpawnOrigin()
    {
        if (m_spawnOrigins.empty())
        {
            if (!m_warnedMissingSpawnOrigins)
            {
                DebugTrace(std::string(kLogPrefix) + "Spawn origin list is empty; using world origin.\n");
                m_warnedMissingSpawnOrigins = true;
            }
            return float3{ 0.0f, 0.0f, 0.0f };
        }

        if (m_nextSpawnOriginIndex >= m_spawnOrigins.size())
        {
            m_nextSpawnOriginIndex = 0;
        }

        return m_spawnOrigins[m_nextSpawnOriginIndex++];
    }

    Transform SweetsManager::BuildSpawnTransform(const float3& origin) const
    {
        Transform transform{};
        transform.position = origin;
        transform.position.y += m_spawnHeightOffset;
        transform.rotation = float3{ 0.0f, 0.0f, 0.0f };
        transform.scale = float3{ 1.0f, 1.0f, 1.0f };

        if (m_enableRandomRotation)
        {
            const auto sampleRotation = [&](float minDeg, float maxDeg)
            {
                const float lo = std::min(minDeg, maxDeg);
                const float hi = std::max(minDeg, maxDeg);
                if (lo == hi)
                {
                    return lo;
                }

                std::uniform_real_distribution<float> dist(lo, hi);
                return dist(m_random);
            };

            const float sampledPitch = sampleRotation(m_randomRotationMinDegrees.x, m_randomRotationMaxDegrees.x);
            const float sampledYaw = sampleRotation(m_randomRotationMinDegrees.y, m_randomRotationMaxDegrees.y);
            const float sampledRoll = sampleRotation(m_randomRotationMinDegrees.z, m_randomRotationMaxDegrees.z);
            transform.rotation = float3{ sampledPitch, sampledYaw, sampledRoll };

            std::ostringstream oss;
            oss << kLogPrefix << "Spawn rotation sampled pitch/yaw/roll(deg)="
                << sampledPitch << ',' << sampledYaw << ',' << sampledRoll
                << " origin=" << origin.x << ',' << origin.y << ',' << origin.z << "\n";
            DebugTrace(oss.str());
        }

        return transform;
    }
}
