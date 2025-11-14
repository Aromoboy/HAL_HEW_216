// CollisionEntity.cpp
#include "CollisionEntity.h"

using namespace Entity;

CollisionEntity::CollisionEntity(std::string a_tag, float3 pos, float3 rot, float3 scl,
    Core::UpdatePhase phase, int priority)
    : CEntity(a_tag, pos, rot, scl, phase, priority)
    , m_collider(nullptr)
{
}

CollisionEntity::~CollisionEntity()
{
}

Collision::ICollider* CollisionEntity::GetCollider()
{
    return m_collider.get();
}