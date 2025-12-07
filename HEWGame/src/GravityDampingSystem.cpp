// GravityDampingSystem.cpp
#include "GravityDampingSystem.h"
#include "Entities/PhysicalEntity.h"

#include <algorithm>
#include <cmath>

using namespace Systems;

GravityDampingSystem::GravityDampingSystem(float gravity, float damping)
    : m_gravity(gravity)
    , m_damping(std::clamp(damping, 0.0f, 1.0f))
{
}

void GravityDampingSystem::Apply(Entities::PhysicalEntity& entity, float /*dt*/)
{
    Physics::Physical& physical = entity.GetPhysical();
    if (!(physical.weight > 0.0f))
    {
        physical.force = float3{ 0.0f, 0.0f, 0.0f };
        return;
    }

    if (entity.IsSleeping())
    {
        physical.force = float3{ 0.0f, 0.0f, 0.0f };
        return;
    }

    const float clampedSystemDamping = std::clamp(m_damping, 0.0f, 0.999f);
    if (physical.damping > 0.0f)
    {
        physical.damping = std::clamp(physical.damping, 0.0f, 0.999f);
    }
    else
    {
        physical.damping = clampedSystemDamping;
    }

    physical.force.y -= m_gravity * physical.weight;
}
