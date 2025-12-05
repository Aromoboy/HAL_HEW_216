#pragma once

#include "Core/UpdateNode.h"

#include <memory>
#include <vector>

#include "Entities/PhysicalEntity.h"

namespace Systems
// PhysicsMachine は Entities 階層に属する物理ボディをまとめて管理する
// UpdateNode。旧 SceneSpace 由来の型は使用せず、Entity 派生のみを受け付ける。
{
    class IPhysicsSystem
    {
    public:
        virtual ~IPhysicsSystem() = default;
        virtual void Apply(Entities::PhysicalEntity& entity, float dt) = 0;
    };

    class IntegrateSystem;

    class PhysicsMachine : public Core::UpdateNode
    {
    public:
        using SystemPtr = std::shared_ptr<IPhysicsSystem>;
        using IntegratorPtr = std::shared_ptr<IntegrateSystem>;

        PhysicsMachine();
        ~PhysicsMachine() override = default;

        void SetSystem(SystemPtr system);
        void SetIntegrator(IntegratorPtr integrator);
        SystemPtr GetSystem() const;
        IntegratorPtr GetIntegrator() const;
        const std::vector<Entities::PhysicalEntity*>& GetBodies() const;



        void RegisterBody(Entities::PhysicalEntity& body);
        void UnregisterBody(Entities::PhysicalEntity& body);
        void ClearBodies();

        void OnUpdate(Core::UpdateContext& ctx) override;

    private:
        SystemPtr m_system;
        IntegratorPtr m_integrator;
        std::vector<Entities::PhysicalEntity*> m_bodies;
    };
}
