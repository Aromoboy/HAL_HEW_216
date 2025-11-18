// ============================================================================
// 概要: GravityDampingSystem に関するインターフェースやデータ構造を宣言し、include から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

// 備考: GravityDampingSystem.h に関する説明を日本語で補足している。
#ifndef __GRAVITYDAMPINGSYSTEM_H__
#define __GRAVITYDAMPINGSYSTEM_H__

#include "Systems/PhysicsMachine.h"
#include <unordered_set>

namespace Systems
{
    class GravityDampingSystem : public IPhysicsSystem
    {
    public:
        GravityDampingSystem(float gravity = 9.8f, float damping = 0.98f);
        virtual ~GravityDampingSystem() = default;

        std::unordered_set<const SceneSpace::PhysicalEntity*> m_loggedKinematicSkips;
        // IPhysicsSystemのApply関数をオーバーライド
        void Apply(SceneSpace::PhysicalEntity& entity, float dt) override;

        void SetGravity(float gravity) { m_gravity = gravity; }
        void SetDamping(float damping) { m_damping = damping; }

        float GetGravity() const { return m_gravity; }
        float GetDamping() const { return m_damping; }

    private:
        float m_gravity;  // 重力加速度
        float m_damping;  // 減衰率
    };
}

#endif