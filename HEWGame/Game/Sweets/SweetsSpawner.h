#pragma once

#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Sweets/SweetsKeyUtils.h"
#include "Sweets/SweetsTypes.h"
#include "SweetsPrototypeManifest.h"
#include "Transform.h"
#include "Vector.h"

namespace SweetsSpace
{
    class SweetsFactory;
    class SweetsRegistry;

    class SweetsSpawner
    {
    public:
        enum class SpawnBlockReason
        {
            BlockedByLiveCap,
            BlockedByPerPointCap,
            NoOrigins,
            NoWeights,
            CoolingDown,
            None,
        };

        struct SpawnDebugSnapshot
        {
            int liveSweets = 0;
            int desiredLiveTarget = 0;
            int liveSweetsCap = 0;
            int queueSize = 0;
            int currentBurstRemaining = 0;
            int pendingTopUpCount = 0;
            std::vector<int> spawnOriginLiveCountsPrimary;
            std::vector<int> spawnOriginLiveCountsSecondary;
            int spawnPerPointCap = 0;
            SpawnBlockReason blockReason = SpawnBlockReason::None;
        };

        using SpawnCallback = std::function<std::shared_ptr<ActiveSweets>(const std::string&, const Transform&)>;

        SweetsSpawner(SweetsFactory& factory, SweetsRegistry& registry, SpawnCallback spawnCallback);

        void RunSpawnScheduler(float deltaSeconds);
        bool LoadSpawnTuningConfig(const std::string& path);
        void ForceSpawnRefresh();
        void SeedInitialSpawns();
        void SetSpawnOrigins(
            const std::vector<float3>& primary,
            const std::vector<float3>& secondary,
            std::optional<float> spawnHeightOffset = std::nullopt);
        void OffsetSpawnOriginsY(float deltaY);
        void SetSpawnIntervalMultiplier(float multiplier);
        void SetSpawnCountBonus(int bonusCount);
        void SetCreamEnabled(bool enabled);
        void SetRandomRotationRangeDegrees(const float3& minDegrees, const float3& maxDegrees);
        void SetLiveSweetsCap(int cap);
        void SetSpawnPointCap(int cap);
        void SetSpawnPerPointInitial(int count);
        void SetSpawnDebugDiagnostics(bool enabled, float rateLimitSeconds);
        bool RequestSupplementalSpawn(int count, const char* context);

        int CountLiveSweets() const;
        int ComputeDesiredLiveTarget(int remainingToStack) const;
        int GetBatchStackTarget() const;
        int GetStackedInBatch() const;

        void OnStateChanged(const std::shared_ptr<ActiveSweets>& sweets, SweetsState previousState, SweetsState newState);
        void OnSweetsStacked(const std::shared_ptr<ActiveSweets>& sweets);
        void ReleaseSpawnOriginOnStacked(const std::shared_ptr<ActiveSweets>& sweets);
        void ReleaseSpawnOrigin(const std::shared_ptr<ActiveSweets>& sweets);
        void OnSweetsRemoved(const std::shared_ptr<ActiveSweets>& sweets, int remainingToStack, int liveAfterRemoval, const char* context);

    private:
        enum class SpawnOriginLane
        {
            Primary,
            Secondary,
            Fallback,
        };

        struct SpawnOriginSelection
        {
            float3 origin{};
            const char* laneLabel = "primary";
            SpawnOriginLane lane = SpawnOriginLane::Primary;
            std::size_t index = 0;
        };

        struct SpawnOriginAssignment
        {
            SpawnOriginLane lane = SpawnOriginLane::Fallback;
            std::size_t index = 0;
        };

        int EnqueueSpawns(int count);
        int EnqueueSupplementalSpawns(int count);
        void EmitSpawnRequests(float deltaSeconds);
        std::string ChooseKindFromActiveConfig();
        void LogSpawnParameters(const SpawnTuningConfig& config) const;
        std::vector<WeightedSweetsType> NormalizeWeights(const std::vector<WeightedSweetsType>& weights) const;
        void ApplyDefaultSpawnConfig();
        std::vector<WeightedSweetsType> BuildDefaultWeights() const;
        std::optional<SpawnOriginSelection> NextSpawnOrigin();
        Transform BuildSpawnTransform(const std::string& normalizedKind, const SpawnOriginSelection& originSelection);
        int SpawnInitialPerPoint(int perPoint);
        void ResetSpawnOriginTracking();
        void RegisterSpawnOrigin(const std::shared_ptr<ActiveSweets>& sweets, const SpawnOriginSelection& selection);
        bool UnregisterSpawnOrigin(const std::shared_ptr<ActiveSweets>& sweets);
        void EnsureRegisteredWeights(SpawnTuningConfig& config,
            const std::unordered_set<std::string>& registeredKindsNormalized,
            const std::unordered_map<SweetsType, float, SweetsTypeHash>& defaultWeightLookup) const;
        SpawnDebugSnapshot BuildDebugSnapshot(int liveSweets, int desiredLiveTarget, SpawnBlockReason reason) const;
        void LogBlockReasonTransition(SpawnBlockReason reason, const SpawnDebugSnapshot& snapshot);
        void LogQueueTransition(std::size_t previousSize, std::size_t currentSize, const SpawnDebugSnapshot& snapshot);
        void LogSpawnDebugEvent(const std::string& key, std::string_view message);
        static const char* ToString(SpawnBlockReason reason);

        SweetsFactory* m_factory;
        SweetsRegistry* m_registry;
        SpawnCallback m_spawnCallback;

        SpawnTuningConfig m_spawnTuningConfig{};
        float m_spawnTimer{ 0.0f };
        float m_burstTimer{ 0.0f };
        std::deque<std::string> m_spawnQueue;
        std::vector<float3> m_spawnOriginsPrimary;
        std::vector<float3> m_spawnOriginsSecondary;
        float m_spawnHeightOffset{ 0.0f };
        bool m_nextSpawnUsesPrimary{ true };
        std::size_t m_spawnOriginCursorPrimary{ 0 };
        std::size_t m_spawnOriginCursorSecondary{ 0 };
        std::vector<int> m_spawnOriginLiveCountsPrimary;
        std::vector<int> m_spawnOriginLiveCountsSecondary;
        std::unordered_map<const ActiveSweets*, SpawnOriginAssignment> m_spawnOriginAssignments;
        float m_activeBurstInterval{ 0.0f };
        int m_activeBurstPlanned{ 0 };
        int m_currentBurstRemaining{ 0 };
        int m_simultaneousLiveTarget{ 0 };
        int m_stackedInBatch{ 0 };
        bool m_batchInitialSpawnDone{ false };
        int m_batchStackTarget{ 0 };
        bool m_batchCoolingDown{ false };
        std::mt19937 m_random;
        int m_pendingTopUpCount{ 0 };
        float m_spawnIntervalMultiplier{ 1.0f };
        int m_spawnCountBonus{ 0 };

        float3 m_randomRotationMinDegrees{ 0.0f, 0.0f, 0.0f };
        float3 m_randomRotationMaxDegrees{ 0.0f, 0.0f, 0.0f };
        int m_liveSweetsCap{ 0 };
        int m_spawnPerPointCap{ 0 };
        int m_spawnPerPointInitial{ 0 };
        bool m_warnedMissingSpawnOrigins{ false };
        std::unordered_set<std::string> m_warnedMissingFactoryPresetKinds;
        bool m_spawnPausedDueToLiveSweets{ false };
        int m_lastLiveSweetsCount{ 0 };
        bool m_creamEnabled{ true };
        bool m_spawnDebugDiagnosticsEnabled{ false };
        float m_spawnDebugRateLimitSeconds{ 1.0f };
        float m_spawnDebugClockSeconds{ 0.0f };
        SpawnBlockReason m_lastSpawnBlockReason{ SpawnBlockReason::None };
        bool m_nextSpawnOriginSelectionFailedLogged{ false };
        struct SpawnRateLimitState
        {
            float lastLogTimeSeconds = -1000.0f;
            int suppressedCount = 0;
        };
        std::unordered_map<std::string, SpawnRateLimitState> m_spawnRateLimitStates;
    };
}
