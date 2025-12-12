#pragma once

#include <memory>
#include <optional>
#include <string>

#include "Entities/PhysicalEntity.h"

namespace SweetsSpace
{
    using Vec3 = float3;

    struct SweetsVisualTransform
    {
        Vec3 position;
        Vec3 rotation;
        Vec3 scale;
    };

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
            int priority = 0);

        ~BaseSweets() override = default;

        void OnUpdate(Core::UpdateContext& ctx) override;
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

        void SetVisualTransform(const SweetsVisualTransform& visual)
        {
            m_visual = visual;
        }

        const SweetsVisualTransform& GetVisualTransform() const
        {
            return m_visual;
        }

    private:
        void SyncFromPhysics();

        bool m_pendingDestroy = false;
        bool m_safeToDelete = true;
        bool m_missingModelWarned = false;
        SweetsVisualTransform m_visual{};
    };
}

