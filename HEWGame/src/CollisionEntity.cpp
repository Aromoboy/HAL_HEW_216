// CollisionEntity.cpp
#include "CollisionEntity.h"

using namespace Entity;

CollisionEntity::CollisionEntity(std::string a_tag, float3 pos, float3 rot, float3 scl,
    std::shared_ptr<Collision::ICollider> collider,
    Core::UpdatePhase phase, int priority)
    : CEntity(a_tag, pos, rot, scl, phase, priority)
    , m_collider(collider)
{
}

CollisionEntity::~CollisionEntity()
{
}

Collision::ICollider* CollisionEntity::GetCollider()
{
    return m_collider.get();
}