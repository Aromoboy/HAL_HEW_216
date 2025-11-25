#pragma once

#include <memory>

#include "Entities/PhysicalEntity.h"
#include "ICollider.h"

namespace SweetsSpace
{
    class BaseSweets : public Entities::PhysicalEntity
    {
    public:
        BaseSweets(const Transform& transform,
            std::shared_ptr<Render::Model> model = nullptr,
            std::shared_ptr<Collision::ICollider> collider = nullptr,
            Collision::CollisionLayer layer = Collision::CollisionLayer::Default,
            Physics::Physical physical = Physics::Physical{},
            Systems::PhysicsMachine* physicsMachine = nullptr,
            Systems::ContactLiteSystem* contactSystem = nullptr,
            Core::UpdatePhase phase = Core::UpdatePhase::PrePhysics,
            int priority = 0);

        ~BaseSweets() override = default;

        void OnUpdate(Core::UpdateContext& ctx) override;
        void Draw(Render::ModelRenderer& renderer, const CameraSpace::ICamera& camera) override;
    };
}

