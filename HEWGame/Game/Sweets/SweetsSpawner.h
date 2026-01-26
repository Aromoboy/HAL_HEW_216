#pragma once

#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <random>
#include <string>
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
        bool RequestSupplementalSpawn(int count, const char* context);

        int CountLiveSweets() const;
        int ComputeDesiredLiveTarget(int remainingToStack) const;
        int GetBatchStackTarget() const;
        int GetStackedInBatch() const;

        void OnStateChanged(const std::shared_ptr<ActiveSweets>& sweets, SweetsState previousState, SweetsState newState);
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
        void UnregisterSpawnOrigin(const std::shared_ptr<ActiveSweets>& sweets);
        void EnsureRegisteredWeights(SpawnTuningConfig& config,
            const std::unordered_set<std::string>& registeredKindsNormalized,
            const std::unordered_map<SweetsType, float, SweetsTypeHash>& defaultWeightLookup) const;

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
    };
}
