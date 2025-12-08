#pragma once

#include <memory>
#include <string>

#include "Entities/PhysicalEntity.h"
#include "ICollider.h"

namespace SweetsSpace
{
    class BaseSweets : public Entities::PhysicalEntity
    {
    public:
        BaseSweets(const std::string& tag,
            const Transform& transform,
            std::shared_ptr<Render::Model> model = nullptr,
            std::shared_ptr<Collision::ICollider> collider = nullptr,
            Collision::CollisionLayer layer = Collision::CollisionLayer::Default,
            Systems::PhysicsMachine* physicsMachine = nullptr,
            Core::UpdatePhase phase = Core::UpdatePhase::PrePhysics,
            int priority = 0);

        ~BaseSweets() override = default;

        void OnUpdate(Core::UpdateContext& ctx) override;
        void Draw(Render::ModelRenderer& renderer, const CameraSpace::ICamera& camera) override;

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

    private:
        bool m_pendingDestroy = false;
        bool m_safeToDelete = true;
    };
}

