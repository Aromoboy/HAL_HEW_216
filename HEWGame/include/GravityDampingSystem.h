// GravityDampingSystem.h
#ifndef __GRAVITYDAMPINGSYSTEM_H__
#define __GRAVITYDAMPINGSYSTEM_H__

#include "Systems/PhysicsMachine.h"

namespace Systems
{
    class GravityDampingSystem : public IPhysicsSystem
    {
    public:
        GravityDampingSystem(float gravity = 9.8f, float damping = 0.98f);
        virtual ~GravityDampingSystem() = default;

        // IPhysicsSystemのApply関数をオーバーライド
        void Apply(Entities::PhysicalEntity& entity, float dt) override;

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
