// GravityDampingSystem.cpp
#include "GravityDampingSystem.h"
#include "PhysicalEntity.h"
#include <algorithm>
#include <cmath>

using namespace Systems;

GravityDampingSystem::GravityDampingSystem(float gravity, float damping)
    : m_gravity(gravity)
    , m_damping(std::clamp(damping, 0.0f, 1.0f))
{
}

void GravityDampingSystem::Apply(SceneSpace::PhysicalEntity& entity, float dt)
{
    if (!(physical.weight > 0.0f))
    {
        physical.velocity = float3{ 0.0f, 0.0f, 0.0f };
        physical.force = float3{ 0.0f, 0.0f, 0.0f };
        return;
    }

    // Physicalコンポーネントを取得
    auto& physical = entity.GetPhysical();

    // 重力加速度を速度に加算（-y方向）
    physical.velocity.y -= m_gravity * dt;

    // 速度に減衰を適用（フレームレート独立）
    const float dampingFactor = std::pow(m_damping, dt);
    physical.velocity.x *= dampingFactor;
    physical.velocity.y *= dampingFactor;
    physical.velocity.z *= dampingFactor;
}
