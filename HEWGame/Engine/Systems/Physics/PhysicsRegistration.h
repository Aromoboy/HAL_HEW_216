// 物理ボディの登録と解除を行う
// シーン側の登録処理と接触通知が参照する

#pragma once

#include <functional>
#include <memory>
#include <unordered_set>
#include <vector>

#include "Systems/Physics/PhysicsMachine.h"

namespace Entities
{
    class PhysicalEntity;
}

namespace Systems
{
    class Box2DContactListener;

    class PhysicsRegistration
    {
    public:
        using ContactCallback = std::function<void(Entities::PhysicalEntity&, Entities::PhysicalEntity&)>;

        explicit PhysicsRegistration(PhysicsMachine& physicsMachine);

        void SetPlayer(Entities::PhysicalEntity* playerEntity);
        void RegisterPhysicsBody(Entities::PhysicalEntity& entity);
        void UnregisterPhysicsBody(Entities::PhysicalEntity* entity);
        void UnregisterAllPhysicsBodies();
        void ValidatePlayerPhysicsRegistration();
        void AddContactHandler(ContactCallback onBegin, ContactCallback onEnd);

    private:
        struct ContactHandler
        {
            ContactCallback onBegin;
            ContactCallback onEnd;
        };

        void DispatchBeginContact(Entities::PhysicalEntity& a, Entities::PhysicalEntity& b);
        void DispatchEndContact(Entities::PhysicalEntity& a, Entities::PhysicalEntity& b);

        PhysicsMachine* m_physicsMachine = nullptr;
        std::shared_ptr<Box2DContactListener> m_contactListener;
        std::unordered_set<Entities::PhysicalEntity*> m_registeredPhysicsBodies;
        std::vector<ContactHandler> m_contactHandlers;
        Entities::PhysicalEntity* m_playerEntity = nullptr;
        bool m_loggedPlayerBodyRecovery = false;
    };
}
