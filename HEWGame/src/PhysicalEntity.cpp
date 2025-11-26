// PhysicalEntity.cpp
#include "Entities/PhysicalEntity.h"

#include <utility>

namespace SceneSpace
{
    PhysicalEntity::PhysicalEntity(const std::string& tag,
        const float3& pos,
        const float3& rot,
        const float3& scl,
        std::shared_ptr<Collision::ICollider> collider,
        Collision::CollisionLayer layer,
        const float3& velocity,
        const float3& force,
        float weight,
        float damping,
        Core::UpdatePhase phase,
        int priority)
        : CollisionEntity(tag, pos, rot, scl, std::move(collider), layer, phase, priority)
        , m_physical(velocity, force, weight, damping)
    {
    }

    PhysicalEntity::~PhysicalEntity() = default;

    Physics::Physical& PhysicalEntity::GetPhysical()
    {
        return m_physical;
    }

    void TranslatePhysicalEntity(PhysicalEntity& entity, const float3& delta)
    {
        entity.CollisionEntity::ApplyTranslation(delta);
    }

}
