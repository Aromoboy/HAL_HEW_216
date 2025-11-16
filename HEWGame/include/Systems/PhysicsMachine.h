#pragma once

#include "Core/UpdateNode.h"

#include <memory>
#include <vector>

#include "PhysicalEntity.h"

namespace Systems
// PhysicsMachine は Entity の完全定義を参照するように更新
// 不完全型エラーを避けるため、SceneSpace::PhysicalEntity を直接扱う
{
    class IPhysicsSystem
 // 物理システムは PhysicalEntity を直接操作
// 以前の Entity::PhysicalEntity から SceneSpace に統一


    {
    public:
        virtual ~IPhysicsSystem() = default;
        virtual void Apply(SceneSpace::PhysicalEntity& entity, float dt) = 0;
    };

    class PhysicsMachine : public Core::UpdateNode
    {
    public:
        using SystemPtr = std::shared_ptr<IPhysicsSystem>;

        PhysicsMachine();
        ~PhysicsMachine() override = default;

        void SetSystem(SystemPtr system);
        SystemPtr GetSystem() const;

// 登録するボディも SceneSpace 名前空間に統一
// Entity の再構成に合わせた変更

        void RegisterBody(SceneSpace::PhysicalEntity& body);
        void UnregisterBody(SceneSpace::PhysicalEntity& body);
        void ClearBodies();

        void OnUpdate(Core::UpdateContext& ctx) override;

    private:
        SystemPtr m_system;
        std::vector<SceneSpace::PhysicalEntity*> m_bodies;
        bool m_loggedMissingSystem;
    };
}