// GravityDampingSystem.cpp
#include "GravityDampingSystem.h"
#include "PhysicalEntity.h"

using namespace Systems;

GravityDampingSystem::GravityDampingSystem(float gravity, float damping)
    : m_gravity(gravity)
    , m_damping(damping)
{
}

void GravityDampingSystem::Apply(SceneSpace::PhysicalEntity& entity, float dt)
{
    // Physicalコンポーネントを取得
    auto& physical = entity.GetPhysical();

    // 重力加速度を速度に加算（-y方向）
    physical.velocity.y -= m_gravity * dt;

    // 速度に減衰を適用
    physical.velocity.x *= m_damping;
    physical.velocity.y *= m_damping;
    physical.velocity.z *= m_damping;
}