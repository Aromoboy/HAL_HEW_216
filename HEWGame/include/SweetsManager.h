#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core/UpdateNode.h"
#include "ICamera.h"
#include "ModelRender.h"
#include "Transform.h"
#include "CollisionLayer.h"

namespace Core
{
    class UpdateScheduler;
}

namespace Systems
{
    class DrawSystem;
    class PhysicsMachine;
}

namespace Collision
{
    class ICollider;
}

class ModelDX;

namespace SweetsSpace
{
    class BaseSweets;

    struct WeightedSweetsType
    {
        std::string kind;
        float weight = 1.0f;
    };

    struct PhaseSpawnConfig
    {
        float spawnIntervalSeconds = 2.5f;
        float burstIntervalSeconds = 0.5f;
        int simultaneousSpawnCount = 1;
        std::vector<WeightedSweetsType> weights;
    };

    enum class DeleteReason : std::uint8_t
    {
        Hit,
        Miss,
    };

    enum class UseOutcome : std::uint8_t
    {
        Hit,
        Miss,
    };

    struct DeleteBatchContext
    {
        int usesRemaining = 0;
    };

    enum class SweetsState : std::uint8_t
    {
        Active,  // 現在制御中または落下中の生成されたお菓子
        Stacked, // タワーの一部であるお菓子を積み重ねることに成功した
        Ground,  // 地面に置かれたお菓子が、拾われるのを待っている
        Carried  // プレイヤーが保持しているため物理・状態遷移を停止
    };

    class SweetsManager : public Core::UpdateNode
    {
    public:
        using SweetsList = std::list<std::shared_ptr<BaseSweets>>;

        using PhaseChangeCallback = std::function<void(int)>;

        SweetsManager();
        ~SweetsManager();

        void OnUpdate(Core::UpdateContext& ctx) override;

        void DrawAll(Render::ModelRenderer& renderer, const std::shared_ptr<CameraSpace::ICamera>& camera);

        SweetsList& GetByState(SweetsState state);
        const SweetsList& GetByState(SweetsState state) const;

        float GetHighestStackedHeight() const;

        void SetState(const std::shared_ptr<BaseSweets>& sweets, SweetsState state);
        void Erase(const std::shared_ptr<BaseSweets>& sweets);
        void ClearAllSweets();

        void SetDrawSystem(Systems::DrawSystem* drawSystem);
        void SetPhysicsMachine(Systems::PhysicsMachine* physicsMachine);
        void SetUpdateScheduler(Core::UpdateScheduler* scheduler);
        void SetPhaseChangeCallback(PhaseChangeCallback callback);
        bool LoadPhaseSpawnConfig(const std::string& path);
        void SetSpawnOrigins(const std::vector<float3>& origins);
        void SetRandomRotationEnabled(bool enabled);
        void SetRandomRotationRangeDegrees(const float3& minDegrees, const float3& maxDegrees);
        void SetRandomScaleEnabled(bool enabled);
        void SetLiveSweetsCap(int cap);
        void ForceSpawnRefresh();
        void NotifyPhaseChanged(int newPhase);
        int GetCurrentPhase() const noexcept;

        std::shared_ptr<BaseSweets> Spawn(const std::string& kind, const Transform& transform);
        std::shared_ptr<BaseSweets> FindNearestCarryable(const float3& position, float radius) const;
        bool TryMarkAsCarried(const std::shared_ptr<BaseSweets>& sweets);
        bool ReleaseCarriedSweets(const std::shared_ptr<BaseSweets>& sweets);

        void SetHorizontalBounds(float minX, float maxX, std::optional<float> margin = std::nullopt);

    private:
        struct SweetsStateHash
        {
            std::size_t operator()(SweetsState state) const noexcept
            {
                return static_cast<std::size_t>(state);
            }
        };

        std::unordered_map<SweetsState, SweetsList, SweetsStateHash> m_byState;
        std::unordered_map<std::shared_ptr<BaseSweets>, SweetsState> m_lookup;

        std::unordered_map<std::string, std::function<std::shared_ptr<BaseSweets>(const Transform&)>> m_spawner;

        Systems::DrawSystem* m_drawSystem;
        Systems::PhysicsMachine* m_physicsMachine;
        Core::UpdateScheduler* m_scheduler;
        PhaseChangeCallback m_phaseChangeCallback;
        int m_currentPhase;
        std::vector<PhaseSpawnConfig> m_phaseConfigs;
        int m_activePhaseConfigIndex;
        float m_spawnTimer;
        float m_burstTimer;
        std::deque<std::string> m_spawnQueue;
        std::vector<float3> m_spawnOrigins;
        float m_spawnHeightOffset;
        float m_activeBurstInterval;
        int m_activeBurstPlanned;
        int m_currentBurstRemaining;
        mutable std::mt19937 m_random;

        bool m_enableRandomRotation;
        float3 m_randomRotationMinDegrees;
        float3 m_randomRotationMaxDegrees;
        bool m_enableRandomScale;
        int m_liveSweetsCap;

        bool m_warnedMissingDrawSystem;
        bool m_warnedMissingPhysicsMachine;
        bool m_warnedMissingScheduler;
        bool m_warnedMissingPhaseCallback;
        bool m_warnedMissingSpawnOrigins;
        mutable bool m_warnedMissingHeightCollider;
        mutable bool m_warnedMissingBoundsCollider;
        bool m_hasHorizontalBounds;
        float m_minHorizontalBound;
        float m_maxHorizontalBound;
        float m_horizontalBoundsMargin;
        bool m_warnedMissingHorizontalBounds;

        std::optional<std::pair<float, float>> GetHorizontalBounds() const;

        void RegisterWithSystems(const std::shared_ptr<BaseSweets>& sweets);
        void RunSpawnScheduler(float deltaSeconds);
        int CountLiveSweets() const;
        void EnqueueSpawns(int count);
        void EmitSpawnRequests(float deltaSeconds);
        std::string ChooseKindFromActiveConfig();
        void OnPhaseConfigurationChanged(int newPhase);
        void LogActiveSpawnParameters(const PhaseSpawnConfig& config, int phaseIndex) const;
        void ApplyDefaultPhaseConfigs();
        std::vector<WeightedSweetsType> BuildDefaultWeights() const;
        float3 NextSpawnOrigin();
        Transform BuildSpawnTransform(const float3& origin) const;
        void UnregisterSweetsFromSystems(const std::shared_ptr<BaseSweets>& sweets, const char* context, bool logWarnings = true);
        void RemoveSweetsEntry(const std::shared_ptr<BaseSweets>& sweets,
            DeleteReason reason,
            const char* context,
            bool logWarnings,
            DeleteBatchContext* batchContext);

        bool m_spawnPausedDueToLiveSweets;
        int m_lastLiveSweetsCount;

        template<typename T>
        std::shared_ptr<BaseSweets> MakeSweets(const std::string& tag,
            const std::shared_ptr<ModelDX>& model,
            const Transform& transform,
            const std::shared_ptr<Collision::ICollider>& collider,
            Collision::CollisionLayer layer,
            const float3& velocity,
            const float3& force,
            float weight,
            float damping,
            Core::UpdatePhase phase,
            int priority)
        {
            if (!model || !collider)
            {
                return nullptr;
            }

            return std::make_shared<T>(tag,
                model,
                transform.position,
                transform.rotation,
                transform.scale,
                collider,
                layer,
                velocity,
                force,
                weight,
                damping,
                phase,
                priority);
        }
    };
}
