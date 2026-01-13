#pragma once

#include <memory>
#include <optional>
#include <string>

#include "Entities/PhysicalEntity.h"
#include "VisualTransform.h"

namespace SweetsSpace
{
    class BaseSweets : public Entities::PhysicalEntity
    {
    public:
        BaseSweets(const std::string& tag,
            const Transform& transform,
            std::shared_ptr<Render::Model> model = nullptr,
            std::optional<Physics::Box2DShapeConfig> shapeConfig = std::nullopt,
            Collision::CollisionLayer layer = Collision::CollisionLayer::Default,
            Systems::PhysicsMachine* physicsMachine = nullptr,
            Core::UpdatePhase phase = Core::UpdatePhase::PrePhysics,
            int priority = 0,
            bool allowMeshPhysics = true);

        ~BaseSweets() override = default;

        void OnUpdate(const Core::UpdateContext& ctx) override;
        void Draw(Render::ModelRenderer& renderer, const ::CameraSpace::ICamera& camera) override;

        Transform GetRenderTransform() const override;

        bool IsPendingDestroy() const noexcept
        {
            return m_pendingDestroy;
        }

        void MarkPendingDestroy() noexcept
        {
            m_pendingDestroy = true;
            m_safeToDelete = false;
        }

        bool IsSafeToDelete() const noexcept
        {
            return m_safeToDelete;
        }

        void MarkSafeToDelete() noexcept
        {
            m_safeToDelete = true;
        }

        void SetVisualTransform(const VisualTransform& visual)
        {
            m_visual = visual;
            RefreshRenderTransform();
        }

        const VisualTransform& GetVisualTransform() const
        {
            return m_visual;
        }

        bool WasEverCarried() const noexcept
        {
            return m_wasEverCarried;
        }

        void SetWasEverCarried(bool value) noexcept
        {
            m_wasEverCarried = value;
        }

        bool IsCemented() const noexcept
        {
            return m_cemented;
        }

        void ApplyCementedPhysics(float gravityScale, float linearDamping, float angularDamping);
        void RestoreCementedPhysics();

    private:
        struct CementedPhysicsState
        {
            float gravityScale{ 0.0f };
            float linearDamping{ 0.0f };
            float angularDamping{ 0.0f };
            bool hasStored{ false };
        };

        void SyncFromPhysics();
        void RefreshRenderTransform();

        bool m_pendingDestroy = false;
        bool m_safeToDelete = true;
        bool m_missingModelWarned = false;
        bool m_wasEverCarried = false;
        bool m_cemented = false;
        CementedPhysicsState m_cementedState{};
        VisualTransform m_visual{};
        Transform m_renderTransform{};
    };
}
