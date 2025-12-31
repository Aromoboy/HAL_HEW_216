#pragma once

#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <random>
#include <string>
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
        bool LoadPhaseSpawnConfig(const std::string& path);
        void ForceSpawnRefresh();
        void SetSpawnOrigins(
            const std::vector<float3>& primary,
            const std::vector<float3>& secondary,
            std::optional<float> spawnHeightOffset = std::nullopt);
        void SetRandomRotationRangeDegrees(const float3& minDegrees, const float3& maxDegrees);
        void SetLiveSweetsCap(int cap);
        void NotifyPhaseChanged(int newPhase);

        int CountLiveSweets() const;
        int ComputeDesiredLiveTarget(int remainingToStack) const;
        int GetBatchStackTarget() const;
        int GetStackedInBatch() const;

        void OnStateChanged(const std::shared_ptr<ActiveSweets>& sweets, SweetsState previousState, SweetsState newState);
        void OnSweetsRemoved(int remainingToStack, int liveAfterRemoval, const char* context);

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
        std::vector<WeightedSweetsType> NormalizeWeights(const std::vector<WeightedSweetsType>& weights) const;
        void LogAutoGenerationPolicy(const char* context) const;
        void ApplyDefaultPhaseConfigs();
        std::vector<WeightedSweetsType> BuildDefaultWeights() const;
        SpawnOriginSelection NextSpawnOrigin();
        Transform BuildSpawnTransform(const std::string& normalizedKind, const SpawnOriginSelection& originSelection);
        void EnsureRegisteredWeights(PhaseSpawnConfig& config, int phaseIndex,
            const std::unordered_set<std::string>& registeredKindsNormalized,
            const std::unordered_map<SweetsType, float, SweetsTypeHash>& defaultWeightLookup) const;

        SweetsFactory* m_factory;
        SweetsRegistry* m_registry;
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
        std::mt19937 m_random;
        bool m_pendingTopUpAfterDrop{ false };

        float3 m_randomRotationMinDegrees{ 0.0f, 0.0f, 0.0f };
        float3 m_randomRotationMaxDegrees{ 0.0f, 0.0f, 0.0f };
        int m_liveSweetsCap{ 0 };
        bool m_warnedMissingSpawnOrigins{ false };
        std::unordered_set<std::string> m_warnedMissingFactoryPresetKinds;
        bool m_spawnPausedDueToLiveSweets{ false };
        int m_lastLiveSweetsCount{ 0 };
    };
}
