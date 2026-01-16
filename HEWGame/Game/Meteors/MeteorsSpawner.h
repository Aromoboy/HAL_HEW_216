#pragma once

#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#include "Meteors/MeteorsKeyUtils.h"
#include "Meteors/MeteorsTypes.h"
#include "MeteorsPrototypeManifest.h"
#include "Transform.h"
#include "Vector.h"

namespace MeteorsSpace
{
    class MeteorsFactory;
    class MeteorsRegistry;

    class MeteorsSpawner
    {
    public:
        using SpawnCallback = std::function<std::shared_ptr<ActiveMeteors>(const std::string&, const Transform&)>;

        MeteorsSpawner(MeteorsFactory& factory, MeteorsRegistry& registry, SpawnCallback spawnCallback);

        void RunSpawnScheduler(float deltaSeconds);
        bool LoadPhaseSpawnConfig(const std::string& path);
        void ForceSpawnRefresh();
        void SetSpawnOrigins(
            const std::vector<float3>& primary,
            const std::vector<float3>& secondary,
            std::optional<float> spawnHeightOffset = std::nullopt);
        void SetRandomRotationRangeDegrees(const float3& minDegrees, const float3& maxDegrees);
        void SetLiveMeteorsCap(int cap);
        void SetCameraBounds(float minX, float maxX, float minY, float maxY);
        void SetPlayerWorldPosition(const float3& position);
        void NotifyPhaseChanged(int newPhase);

        int CountLiveMeteors() const;
        int ComputeDesiredLiveTarget(int remainingToStack) const;
        int GetBatchStackTarget() const;
        int GetStackedInBatch() const;
        void DecrementStackedCount();
        void RecountStackedInBatch();

        void OnStateChanged(const std::shared_ptr<ActiveMeteors>& meteors, MeteorsState previousState, MeteorsState newState);
        void OnMeteorsRemoved(int remainingToStack, int liveAfterRemoval, const char* context);

        const std::vector<PhaseSpawnConfig>& GetPhaseConfigs() const;
        int GetActivePhaseConfigIndex() const;

    private:
        struct SpawnOriginSelection
        {
            float3 origin{};
            const char* laneLabel = "primary";
        };

        void EnqueueSpawns(int count);
        void EmitSpawnRequests(float deltaSeconds);
        std::string ChooseKindFromActiveConfig();
        void OnPhaseConfigurationChanged(int newPhase);
        void LogActiveSpawnParameters(const PhaseSpawnConfig& config, int phaseIndex) const;
        void EnsurePhaseConfigAvailable(int phaseIndex);
        PhaseSpawnConfig GenerateAutoPhaseConfig(const PhaseSpawnConfig& previous, int previousIndex) const;
        std::vector<WeightedMeteorsType> NormalizeWeights(const std::vector<WeightedMeteorsType>& weights) const;
        void LogAutoGenerationPolicy(const char* context) const;
        void ApplyDefaultPhaseConfigs();
        std::vector<WeightedMeteorsType> BuildDefaultWeights() const;
        SpawnOriginSelection NextSpawnOrigin();
        Transform BuildSpawnTransform(const std::string& normalizedKind, const SpawnOriginSelection& originSelection);
        void EnsureRegisteredWeights(PhaseSpawnConfig& config, int phaseIndex,
            const std::unordered_set<std::string>& registeredKindsNormalized,
            const std::unordered_map<MeteorsType, float, MeteorsTypeHash>& defaultWeightLookup) const;

        MeteorsFactory* m_factory;
        MeteorsRegistry* m_registry;
        SpawnCallback m_spawnCallback;

        std::vector<PhaseSpawnConfig> m_phaseConfigs;
        int m_activePhaseConfigIndex{ -1 };
        int m_currentPhase{ 0 };
        float m_spawnTimer{ 0.0f };
        float m_burstTimer{ 0.0f };
        std::deque<std::string> m_spawnQueue;
        std::vector<float3> m_spawnOriginsPrimary;
        std::vector<float3> m_spawnOriginsSecondary;
        float m_spawnHeightOffset{ 0.0f };
        float m_spawnCameraOffset{ 0.0f };
        bool m_nextSpawnUsesPrimary{ true };
        std::size_t m_spawnOriginCursorPrimary{ 0 };
        std::size_t m_spawnOriginCursorSecondary{ 0 };
        float m_activeBurstInterval{ 0.0f };
        int m_activeBurstPlanned{ 0 };
        int m_currentBurstRemaining{ 0 };
        int m_simultaneousLiveTarget{ 0 };
        int m_stackedInBatch{ 0 };
        bool m_batchInitialSpawnDone{ false };
        int m_batchStackTarget{ 0 };
        bool m_batchCoolingDown{ false };
        float m_autoSpawnIntervalFactor{ 0.0f };
        float m_autoBurstIntervalFactor{ 0.0f };
        int m_autoSimultaneousIncrement{ 0 };
        int m_autoSimultaneousCap{ 0 };
        float m_growthElapsedSeconds{ 0.0f };
        float m_growthIntervalSeconds{ 0.0f };
        int m_growthStep{ 0 };
        int m_growthStepsApplied{ 0 };
        std::mt19937 m_random;
        bool m_pendingTopUpAfterDrop{ false };

        float3 m_randomRotationMinDegrees{ 0.0f, 0.0f, 0.0f };
        float3 m_randomRotationMaxDegrees{ 0.0f, 0.0f, 0.0f };
        int m_liveMeteorsCap{ 0 };
        bool m_hasCameraBounds{ false };
        float m_cameraMinX{ 0.0f };
        float m_cameraMaxX{ 0.0f };
        float m_cameraMinY{ 0.0f };
        float m_cameraMaxY{ 0.0f };
        float3 m_cachedPlayerPosition{ 0.0f, 0.0f, 0.0f };
        bool m_hasCachedPlayerPosition{ false };
        bool m_warnedMissingSpawnOrigins{ false };
        std::unordered_set<std::string> m_warnedMissingFactoryPresetKinds;
        bool m_spawnPausedDueToLiveMeteors{ false };
        int m_lastLiveMeteorsCount{ 0 };
    };
}
