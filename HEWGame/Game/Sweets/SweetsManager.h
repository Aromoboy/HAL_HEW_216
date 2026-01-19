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
#include "SweetsPrototypeManifest.h"
#include "Sweets/SweetsTypes.h"
#include "Sweets/SweetsContacts.h"
#include "Sweets/SweetsFactory.h"
#include "Sweets/SweetsLifecycle.h"
#include "Sweets/SweetsRegistry.h"
#include "Sweets/SweetsRenderer.h"
#include "Sweets/SweetsSpawner.h"
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

namespace SweetsSpace
{
    class LateUpdateNode;
    using WeightedSweetsType = SweetsSpace::WeightedSweetsType;
    using PhaseSpawnConfig = SweetsSpace::PhaseSpawnConfig;
    using ColliderShape = SweetsSpace::ColliderShape;
    using ColliderPreset = SweetsSpace::ColliderPreset;
    using DeleteReason = SweetsSpace::DeleteReason;
    using UseOutcome = SweetsSpace::UseOutcome;
    using PickupResult = SweetsSpace::PickupResult;
    using PickupFailReason = SweetsSpace::PickupFailReason;
    using PickupQueryResult = SweetsSpace::PickupQueryResult;
    using DeleteBatchContext = SweetsSpace::DeleteBatchContext;
    using SweetsState = SweetsSpace::SweetsState;

    class SweetsManager : public Core::UpdateNode
    {
    public:
        using SweetsList = std::list<std::shared_ptr<ActiveSweets>>;

        using PhaseChangeCallback = std::function<void(int)>;

        using ColliderSource = SweetsSpace::ColliderSource;
        using ColliderBuildResult = SweetsSpace::ColliderBuildResult;
        using FactoryPreset = SweetsSpace::FactoryPreset;

        SweetsManager(Systems::DrawSystem& drawSystem,
            Systems::PhysicsMachine& physicsMachine,
            Core::UpdateScheduler& scheduler);
        ~SweetsManager();

        void OnUpdate(const Core::UpdateContext& ctx) override;
        void OnLateUpdate(const Core::UpdateContext& ctx);
        void Update(const Core::UpdateContext& ctx);
        void DrawAll(Render::ModelRenderer& renderer, const std::shared_ptr<::CameraSpace::ICamera>& camera);

        SweetsList& GetByState(SweetsState state);
        const SweetsList& GetByState(SweetsState state) const;

        float GetHighestStackedHeight() const;

        void SetState(const std::shared_ptr<ActiveSweets>& sweets, SweetsState state);
        void Erase(const std::shared_ptr<ActiveSweets>& sweets);
        void ClearAllSweets();

        void SetPhaseChangeCallback(PhaseChangeCallback callback);
        bool LoadPhaseSpawnConfig(const std::string& path);
        void SetSpawnOrigins(
            const std::vector<float3>& primary,
            const std::vector<float3>& secondary,
            std::optional<float> spawnHeightOffset = std::nullopt);
        void OffsetSpawnOriginsY(float deltaY);
        void SetRandomRotationRangeDegrees(const float3& minDegrees, const float3& maxDegrees);
        void SetLiveSweetsCap(int cap);
        void SetForceGeneratedColliders(bool force);
        void SetForceConfigColliders(bool force, const std::vector<std::string>& kinds = {});
        void SetColliderConfigOverridesEnabled(bool enabled);
        void ForceSpawnRefresh();
        void NotifyPhaseChanged(int newPhase);
        Core::IUpdatable* GetLateUpdateNode();
        const Core::IUpdatable* GetLateUpdateNode() const;
        int GetCurrentPhase() const noexcept;
        std::vector<std::pair<std::string, FactoryPreset>> EnumerateFactoryPresets() const;

        std::shared_ptr<ActiveSweets> Spawn(const std::string& kind, const Transform& transform);
        PickupQueryResult FindNearestCarryable(
            const float3& pickupCenter,
            float halfWidth,
            float halfHeight,
            float halfDepth,
            float depthThreshold,
            const std::vector<std::shared_ptr<ActiveSweets>>& exclude,
            float maxDistance) const;
        bool TryMarkAsCarried(const std::shared_ptr<ActiveSweets>& sweets);
        bool ReleaseCarriedSweets(const std::shared_ptr<ActiveSweets>& sweets);

        void SetHorizontalBounds(float minX, float maxX, std::optional<float> margin = std::nullopt);
        void SetWorldBounds(float minX, float maxX, float minY, std::optional<float> margin = std::nullopt);
        void SetGeneratedColliderThicknessPadding(float factor);
        void SetGeneratedColliderMinThickness(float thickness);
        void OnContactBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b, const Systems::ContactEventInfo& info);
        void OnContactEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b, const Systems::ContactEventInfo& info);
        bool HasSupportingContact(const ActiveSweets& sweets, bool* foundationOnly = nullptr) const;
        bool HasEverCollidedWithSweets(const ActiveSweets& sweets) const;
        bool IsAnchoredToCake(const ActiveSweets& sweets) const;
        int GetSweetsCount() const;
        void OnSweetsPassedGuideline(const std::shared_ptr<ActiveSweets>& sweets);
        bool GetGlueDebugSnapshot(SweetsContacts::GlueDebugSnapshot& outSnapshot) const;
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

        SweetsRegistry m_registry;
        SweetsFactory m_factory;
        SweetsSpawner m_spawner;
        SweetsContacts m_contacts;
        SweetsLifecycle m_lifecycle;
        SweetsRenderer m_renderer;
    };

}
