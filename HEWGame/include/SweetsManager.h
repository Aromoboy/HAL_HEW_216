#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Core/UpdateNode.h"
#include "ICamera.h"
#include "ModelDX.h"
#include "ModelRender.h"
#include "Transform.h"
#include "Vector.h"
#include "CollisionLayer.h"
#include "Physical.h"
#include "ColliderConfigLoader.h"
#include "SweetsPrototypeManifest.h"
#include "Systems/Box2DMeshBuilder.h"
#include "Systems/PhysicsMachine.h"

namespace Core
{
    class UpdateScheduler;
}

namespace Systems
{
    class DrawSystem;
    class PhysicsMachine;
}

namespace Entities
{
    class PhysicalEntity;
}

namespace Collision
{
    class ICollider;
}

namespace SweetsSpace
{
    class BaseSweets;
    using ActiveSweets = BaseSweets;
    class LateUpdateNode;

    struct WeightedSweetsType
    {
        SweetsType kind{ SweetsType::Cake };
        float weight = 1.0f;
    };

    struct PhaseSpawnConfig
    {
        float spawnIntervalSeconds = 2.5f;
        float burstIntervalSeconds = 0.5f;
        int simultaneousSpawnCount = 1;
        int simultaneousLiveTarget = 1;
        std::vector<WeightedSweetsType> weights;
    };

    enum class ColliderShape : std::uint8_t
    {
        Box,
        Capsule,
    };

    struct ColliderPreset
    {
        ColliderShape shape{ ColliderShape::Box };
        float3 padding{ 0.0f, 0.0f, 0.0f };
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
        using SweetsList = std::list<std::shared_ptr<ActiveSweets>>;

        using PhaseChangeCallback = std::function<void(int)>;

        struct FactoryPreset
        {
            float spawnWeight = 1.0f;
            float3 minScale{ 1.0f, 1.0f, 1.0f };
            float3 maxScale{ 1.0f, 1.0f, 1.0f };
            ColliderPreset colliderPreset;
        };

        SweetsManager();
        ~SweetsManager();

        void OnUpdate(Core::UpdateContext& ctx) override;
        void OnLateUpdate(Core::UpdateContext& ctx);

        void DrawAll(Render::ModelRenderer& renderer, const std::shared_ptr<::CameraSpace::ICamera>& camera);

        SweetsList& GetByState(SweetsState state);
        const SweetsList& GetByState(SweetsState state) const;

        float GetHighestStackedHeight() const;

        void SetState(const std::shared_ptr<ActiveSweets>& sweets, SweetsState state);
        void Erase(const std::shared_ptr<ActiveSweets>& sweets);
        void ClearAllSweets();

        void SetDrawSystem(Systems::DrawSystem* drawSystem);
        void SetPhysicsMachine(Systems::PhysicsMachine* physicsMachine);
        void SetUpdateScheduler(Core::UpdateScheduler* scheduler);
        void SetPhaseChangeCallback(PhaseChangeCallback callback);
        bool LoadPhaseSpawnConfig(const std::string& path);
        void SetSpawnOrigins(const std::vector<float3>& origins);
        void SetRandomRotationRangeDegrees(const float3& minDegrees, const float3& maxDegrees);
        void SetLiveSweetsCap(int cap);
        void SetForceGeneratedColliders(bool force);
        void SetColliderConfigOverridesEnabled(bool enabled);
        void ForceSpawnRefresh();
        void NotifyPhaseChanged(int newPhase);
        Core::IUpdatable* GetLateUpdateNode();
        const Core::IUpdatable* GetLateUpdateNode() const;
        int GetCurrentPhase() const noexcept;
        std::vector<std::pair<std::string, FactoryPreset>> EnumerateFactoryPresets() const;

        std::shared_ptr<ActiveSweets> Spawn(const std::string& kind, const Transform& transform);
        std::shared_ptr<ActiveSweets> FindNearestCarryable(const float3& position, float halfWidth, float halfHeight) const;
        bool TryMarkAsCarried(const std::shared_ptr<ActiveSweets>& sweets);
        bool ReleaseCarriedSweets(const std::shared_ptr<ActiveSweets>& sweets);

        void SetHorizontalBounds(float minX, float maxX, std::optional<float> margin = std::nullopt);
        void SetGeneratedColliderThicknessPadding(float factor);
        void SetGeneratedColliderMinThickness(float thickness);
        void OnContactBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b);
        void OnContactEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b);
        bool HasSupportingContact(const ActiveSweets& sweets) const;
        int GetSweetsCount() const;
    private:
        struct SweetsStateHash
        {
            std::size_t operator()(SweetsState state) const noexcept
            {
                return static_cast<std::size_t>(state);
            }
        };

        std::unordered_map<SweetsState, SweetsList, SweetsStateHash> m_byState;
        std::unordered_map<std::shared_ptr<ActiveSweets>, SweetsState> m_lookup;

        std::unordered_set<std::shared_ptr<ActiveSweets>> m_pendingLateDeletion;

        std::unordered_map<std::string, std::function<std::shared_ptr<ActiveSweets>(const Transform&)>> m_spawner;
        std::map<std::string, FactoryPreset> m_factoryPresets;

        std::map<std::string, Collision::ColliderDefinition> m_colliderDefinitions;
        struct GeneratedCollider
        {
            std::vector<float2> hull;
            float2 centroidOffset{ 0.0f, 0.0f };
            float thickness = 1.0f;
        };
        std::map<std::string, GeneratedCollider> m_generatedColliders;
        std::map<std::string, Systems::Box2DMeshFixtures> m_box2dFixtures;

        Systems::DrawSystem* m_drawSystem;
        Systems::PhysicsMachine* m_physicsMachine;
        Core::UpdateScheduler* m_scheduler;
        std::unique_ptr<LateUpdateNode> m_lateUpdateNode;
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
        int m_simultaneousLiveTarget;
        int m_stackedInBatch;
        bool m_batchInitialSpawnDone;
        int m_batchStackTarget;
        bool m_batchCoolingDown;
        mutable std::mt19937 m_random;
        bool m_pendingTopUpAfterDrop;

        float3 m_randomRotationMinDegrees;
        float3 m_randomRotationMaxDegrees;
        int m_liveSweetsCap;

        bool m_warnedMissingDrawSystem;
        bool m_warnedMissingPhysicsMachine;
        bool m_warnedMissingScheduler;
        bool m_warnedMissingPhaseCallback;
        bool m_warnedMissingSpawnOrigins;
        bool m_loggedContactNotification;
        std::unordered_set<std::string> m_warnedMissingColliderDefinitionKinds;
        std::unordered_set<std::string> m_warnedMissingFactoryPresetKinds;
        bool m_warnedMissingGeneratedHull;
        bool m_hasHorizontalBounds;
        float m_minHorizontalBound;
        float m_maxHorizontalBound;
        float m_horizontalBoundsMargin;
        float m_generatedColliderThicknessPadding;
        float m_generatedColliderMinThickness;
        bool m_forceGeneratedColliders;
        bool m_colliderConfigOverridesEnabled;
        bool m_warnedMissingHorizontalBounds;
        std::unordered_set<ActiveSweets*> m_registeredPhysicsBodies;
        struct ContactState
        {
            int foundationContacts{ 0 };
            int sweetsContacts{ 0 };
        };
        std::unordered_map<ActiveSweets*, ContactState> m_contactStates;

        std::optional<std::pair<float, float>> GetHorizontalBounds() const;

        void RegisterWithSystems(const std::shared_ptr<ActiveSweets>& sweets);
        void RunSpawnScheduler(float deltaSeconds);
        int CountLiveSweets() const;
        int ComputeDesiredLiveTarget(int remainingToStack) const;
        void EnqueueSpawns(int count);
        void EmitSpawnRequests(float deltaSeconds);
        std::string ChooseKindFromActiveConfig();
        void OnPhaseConfigurationChanged(int newPhase);
        void LogActiveSpawnParameters(const PhaseSpawnConfig& config, int phaseIndex) const;
        void EnsureRegisteredWeights(PhaseSpawnConfig& config, int phaseIndex,
            const std::unordered_set<std::string>& registeredKindsNormalized,
            const std::unordered_map<SweetsType, float, SweetsTypeHash>& defaultWeightLookup) const;
        void ApplyDefaultPhaseConfigs();
        std::vector<WeightedSweetsType> BuildDefaultWeights() const;
        float3 NextSpawnOrigin();
        Transform BuildSpawnTransform(const float3& origin) const;
        void UnregisterSweetsFromSystems(const std::shared_ptr<ActiveSweets>& sweets, const char* context, bool logWarnings = true);
        void RemoveSweetsEntry(const std::shared_ptr<ActiveSweets>& sweets,
            DeleteReason reason,
            const char* context,
            bool logWarnings,
            DeleteBatchContext* batchContext);
        void UpdateContactStateBegin(ActiveSweets* sweets, Entities::PhysicalEntity* other);
        void UpdateContactStateEnd(ActiveSweets* sweets, Entities::PhysicalEntity* other);
        void ClearContactState(ActiveSweets* sweets);

        static std::optional<FactoryPreset> BuildDefaultFactoryPreset(SweetsType type);
        static bool ParseFactoryRow(const std::vector<std::string>& tokens, FactoryPreset& outPreset, size_t lineNumber);
        static std::map<std::string, FactoryPreset> LoadFactoryPresets(bool* parseError);
        std::string ResolveKindKey(const std::string& key) const;
        std::map<std::string, Collision::ColliderDefinition>::const_iterator FindColliderDefinition(const std::string& key) const;

        bool m_spawnPausedDueToLiveSweets;
        int m_lastLiveSweetsCount;

        template<typename T>
        std::shared_ptr<ActiveSweets> MakeSweets(
            const std::string& tag,
            const std::shared_ptr<Render::Model>& model,
            const Transform& transform,
            const std::optional<Physics::Box2DShapeConfig>& shapeConfig,
            Collision::CollisionLayer layer,
            Core::UpdatePhase phase,
            int priority)
        {
            if (!model)
                return nullptr;

            auto sweets = std::make_shared<T>(
                tag,
                transform,
                model,
                shapeConfig,
                layer,
                m_physicsMachine,
                phase,
                priority
            );

            if (!sweets)
                return nullptr;

            return sweets;
        }
    };

}
