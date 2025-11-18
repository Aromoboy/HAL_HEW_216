// ============================================================================
// 概要: GravityDampingSystem の挙動や関連ロジックを実装し、src に属する機能を具体化している。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: src 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

// 備考: GravityDampingSystem.cpp に関する説明を日本語で補足している。
#include "GravityDampingSystem.h"
#include "PhysicalEntity.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace
{
    void DebugTrace(const std::string& message)
    {
#if defined(_WIN32) && defined(_DEBUG)
        ::OutputDebugStringA(message.c_str());
#else
        (void)message;
#endif
    }
}

using namespace Systems;

GravityDampingSystem::GravityDampingSystem(float gravity, float damping)
    : m_gravity(gravity)
    , m_damping(std::clamp(damping, 0.0f, 1.0f))
{
}

void GravityDampingSystem::Apply(SceneSpace::PhysicalEntity& entity, float dt)
{
    // Physicalコンポーネントを取得
    auto& physical = entity.GetPhysical();

    if (!(physical.weight > 0.0f))
    {
#if defined(_DEBUG)
        if (m_loggedKinematicSkips.insert(&entity).second)
        {
            std::ostringstream oss;
            oss << "[GravityDampingSystem] Skipping kinematic entity tag='" << entity.GetTag()
                << "' weight=" << physical.weight << "\n";
            DebugTrace(oss.str());
        }
#endif
        physical.velocity = float3{ 0.0f, 0.0f, 0.0f };
        physical.force = float3{ 0.0f, 0.0f, 0.0f };
        return;
    }

    m_loggedKinematicSkips.erase(&entity);

    // 重力加速度を速度に加算（-y方向）
    physical.velocity.y -= m_gravity * dt;

    // 速度に減衰を適用（フレームレート独立）
    const float dampingFactor = std::pow(m_damping, dt);
    physical.velocity.x *= dampingFactor;
    physical.velocity.y *= dampingFactor;
    physical.velocity.z *= dampingFactor;
}
