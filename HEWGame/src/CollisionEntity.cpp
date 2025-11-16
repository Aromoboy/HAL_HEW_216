// CollisionEntity.cpp
#include "CollisionEntity.h"

namespace SceneSpace
{
    CollisionEntity::CollisionEntity(const std::string& tag,
        const float3& pos,
        const float3& rot,
        const float3& scl,
        std::shared_ptr<Collision::ICollider> collider,
        Core::UpdatePhase phase,
        int priority)
        : Entity(tag, pos, rot, scl, phase, priority)
        , m_collider(std::move(collider))
    {
    }

    CollisionEntity::~CollisionEntity() = default;

    Collision::ICollider* CollisionEntity::GetCollider()
    {
        return m_collider.get();
    }
}