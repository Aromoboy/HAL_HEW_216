// PhysicalEntity.cpp
#include "PhysicalEntity.h"

using namespace Entity;

PhysicalEntity::PhysicalEntity(std::string a_tag, float3 pos, float3 rot, float3 scl,
    std::shared_ptr<Collision::ICollider> collider,
    float3 velocity, float3 force,
    float weight, float damping,
    Core::UpdatePhase phase, int priority)
    : CollisionEntity(a_tag, pos, rot, scl, collider, phase, priority)
    , m_physical(velocity, force, weight, damping)
{
}

PhysicalEntity::~PhysicalEntity()
{
}

Physics::Physical& PhysicalEntity::GetPhysical()
{
    return m_physical;
}