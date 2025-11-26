#include "Sweets/BaseSweets.h"

namespace SweetsSpace
{
    BaseSweets::BaseSweets(const std::string& tag,
        const Transform& transform,
        std::shared_ptr<Render::Model> model,
        std::shared_ptr<Collision::ICollider> collider,
        Collision::CollisionLayer layer,
        Physics::Physical physical,
        Systems::PhysicsMachine* physicsMachine,
        Systems::ContactLiteSystem* contactSystem,
        Core::UpdatePhase phase,
        int priority)
        : Entities::PhysicalEntity(transform,
            std::move(collider),
            layer,
            physical,
            std::move(model),
            physicsMachine,
            contactSystem,
            phase,
            priority,
            tag)
    {
    }

    void BaseSweets::OnUpdate(Core::UpdateContext& ctx)
    {
        (void)ctx;
    }

    void BaseSweets::Draw(Render::ModelRenderer& renderer, const CameraSpace::ICamera& camera)
    {
        (void)renderer;
        (void)camera;
    }
}

