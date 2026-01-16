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
#include <limits>

#include "Core/UpdateNode.h"
#include "ICamera.h"
#include "ModelDX.h"
#include "ModelRender.h"
#include "Transform.h"
#include "Vector.h"
#include "CollisionLayer.h"
#include "Physical.h"
#include "ColliderConfigLoader.h"
#include "MeteorsPrototypeManifest.h"
#include "Meteors/MeteorsTypes.h"
#include "Meteors/MeteorsContacts.h"
#include "Meteors/MeteorsFactory.h"
#include "Meteors/MeteorsLifecycle.h"
#include "Meteors/MeteorsRegistry.h"
#include "Meteors/MeteorsRenderer.h"
#include "Meteors/MeteorsSpawner.h"
#include "Systems/Physics/Box2DMeshBuilder.h"
#include "Systems/Physics/ContactEventInfo.h"
#include "Systems/Physics/PhysicsMachine.h"

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

namespace MeteorsSpace
{
    class LateUpdateNode;
    using WeightedMeteorsType = MeteorsSpace::WeightedMeteorsType;
    using PhaseSpawnConfig = MeteorsSpace::PhaseSpawnConfig;
    using ColliderShape = MeteorsSpace::ColliderShape;
    using ColliderPreset = MeteorsSpace::ColliderPreset;
    using DeleteReason = MeteorsSpace::DeleteReason;
    using UseOutcome = MeteorsSpace::UseOutcome;
    using PickupResult = MeteorsSpace::PickupResult;
    using PickupFailReason = MeteorsSpace::PickupFailReason;
    using PickupQueryResult = MeteorsSpace::PickupQueryResult;
    using DeleteBatchContext = MeteorsSpace::DeleteBatchContext;
    using MeteorsState = MeteorsSpace::MeteorsState;

    class MeteorsManager : public Core::UpdateNode
    {
    public:
        using MeteorsList = std::list<std::shared_ptr<ActiveMeteors>>;

        using PhaseChangeCallback = std::function<void(int)>;

        using ColliderSource = MeteorsSpace::ColliderSource;
        using ColliderBuildResult = MeteorsSpace::ColliderBuildResult;
        using FactoryPreset = MeteorsSpace::FactoryPreset;

        MeteorsManager(Systems::DrawSystem& drawSystem,
            Systems::PhysicsMachine& physicsMachine,
            Core::UpdateScheduler& scheduler);
        ~MeteorsManager();

        void OnUpdate(const Core::UpdateContext& ctx) override;
        void OnLateUpdate(const Core::UpdateContext& ctx);
        void Update(const Core::UpdateContext& ctx);
        void DrawAll(Render::ModelRenderer& renderer, const std::shared_ptr<::CameraSpace::ICamera>& camera);

        MeteorsList& GetByState(MeteorsState state);
        const MeteorsList& GetByState(MeteorsState state) const;
        std::shared_ptr<ActiveMeteors> FindByEntity(Entities::PhysicalEntity* entity) const;

        float GetHighestStackedHeight() const;

        void SetState(const std::shared_ptr<ActiveMeteors>& meteors, MeteorsState state);
        void Erase(const std::shared_ptr<ActiveMeteors>& meteors);
        void ClearAllMeteors();

        void SetPhaseChangeCallback(PhaseChangeCallback callback);
        bool LoadPhaseSpawnConfig(const std::string& path);
        void SetSpawnOrigins(
            const std::vector<float3>& primary,
            const std::vector<float3>& secondary,
            std::optional<float> spawnHeightOffset = std::nullopt);
        void SetRandomRotationRangeDegrees(const float3& minDegrees, const float3& maxDegrees);
        void SetLiveMeteorsCap(int cap);
        void SetForceGeneratedColliders(bool force);
        void SetForceConfigColliders(bool force, const std::vector<std::string>& kinds = {});
        void SetColliderConfigOverridesEnabled(bool enabled);
        void ForceSpawnRefresh();
        void NotifyPhaseChanged(int newPhase);
        Core::IUpdatable* GetLateUpdateNode();
        const Core::IUpdatable* GetLateUpdateNode() const;
        int GetCurrentPhase() const noexcept;
        std::vector<std::pair<std::string, FactoryPreset>> EnumerateFactoryPresets() const;

        std::shared_ptr<ActiveMeteors> Spawn(const std::string& kind, const Transform& transform);
        PickupQueryResult FindNearestCarryable(
            const float3& pickupCenter,
            float halfWidth,
            float halfHeight,
            float halfDepth,
            float depthThreshold,
            const std::vector<std::shared_ptr<ActiveMeteors>>& exclude,
            float maxDistance) const;
        bool TryMarkAsCarried(const std::shared_ptr<ActiveMeteors>& meteors);
        bool ReleaseCarriedMeteors(const std::shared_ptr<ActiveMeteors>& meteors);

        void SetHorizontalBounds(float minX, float maxX, std::optional<float> margin = std::nullopt);
        void SetCameraBounds(float minX, float maxX, float minY, float maxY);
        void SetGeneratedColliderThicknessPadding(float factor);
        void SetGeneratedColliderMinThickness(float thickness);
        void SetPlayerWorldPosition(const float3& position);
        std::optional<MeteorsType> RegisterProjectileHit(const std::shared_ptr<ActiveMeteors>& meteors);
        void OnContactBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b, const Systems::ContactEventInfo& info);
        void OnContactEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b, const Systems::ContactEventInfo& info);
        bool HasSupportingContact(const ActiveMeteors& meteors, bool* foundationOnly = nullptr) const;
        bool HasEverCollidedWithMeteors(const ActiveMeteors& meteors) const;
        int GetMeteorsCount() const;
        bool GetGlueDebugSnapshot(MeteorsContacts::GlueDebugSnapshot& outSnapshot) const;
    private:
        Systems::DrawSystem* m_drawSystem;
        Systems::PhysicsMachine* m_physicsMachine;
        Core::UpdateScheduler* m_scheduler;
        std::unique_ptr<LateUpdateNode> m_lateUpdateNode;
        PhaseChangeCallback m_phaseChangeCallback;
        int m_currentPhase;
        bool m_warnedMissingDrawSystem;
        bool m_warnedMissingPhysicsMachine;
        bool m_warnedMissingScheduler;
        bool m_warnedMissingPhaseCallback;

        MeteorsRegistry m_registry;
        MeteorsFactory m_factory;
        MeteorsSpawner m_spawner;
        MeteorsContacts m_contacts;
        MeteorsLifecycle m_lifecycle;
        MeteorsRenderer m_renderer;
        float3 m_cachedPlayerPosition{ 0.0f, 0.0f, 0.0f };
        bool m_hasCachedPlayerPosition{ false };
        std::mt19937 m_random;
        std::unordered_map<ActiveMeteors*, int> m_meteorsHitCounts;
    };

}
