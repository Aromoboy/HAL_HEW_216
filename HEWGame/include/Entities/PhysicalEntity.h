#pragma once

#include "Entities/CollisionEntity.h"
#include "Physical.h"

namespace Systems
{
    class PhysicsMachine;
}

namespace Entities
{
    class PhysicalEntity : public CollisionEntity
    {
    public:
        PhysicalEntity(const Transform& transform,
            std::shared_ptr<Collision::ICollider> collider = nullptr,
            Collision::CollisionLayer layer = Collision::CollisionLayer::Default,
            Physics::Physical physical = Physics::Physical{},
            std::shared_ptr<Render::Model> model = nullptr,
            Systems::PhysicsMachine* physicsMachine = nullptr,
            Systems::ContactLiteSystem* contactSystem = nullptr,
            Core::UpdatePhase phase = Core::UpdatePhase::PrePhysics,
            int priority = 0);
        ~PhysicalEntity() override = default;

        Physics::Physical& GetPhysical();
        const Physics::Physical& GetPhysical() const;

        Systems::PhysicsMachine* GetPhysicsMachine() const;

    protected:
        Physics::Physical m_physical;
        Systems::PhysicsMachine* m_physicsMachine;
    };
}

