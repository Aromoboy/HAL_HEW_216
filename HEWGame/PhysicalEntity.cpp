// PhysicalEntity.cpp
#include "PhysicalEntity.h"

using namespace Entity;

PhysicalEntity::PhysicalEntity(std::string a_tag, float3 pos, float3 rot, float3 scl,
    Core::UpdatePhase phase, int priority)
    : CollisionEntity(a_tag, pos, rot, scl, phase, priority)
{
}

PhysicalEntity::~PhysicalEntity()
{
}

Physics::Physical& PhysicalEntity::GetPhysical()
{
    return m_physical;
}