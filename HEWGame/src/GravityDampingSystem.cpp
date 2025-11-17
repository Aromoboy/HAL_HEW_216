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

    // 速度に減衰を適用（フレームレート独立）
    float dampingFactor = pow(m_damping, dt);
    physical.velocity.x *= dampingFactor;
    physical.velocity.y *= dampingFactor;
    physical.velocity.z *= dampingFactor;
}
