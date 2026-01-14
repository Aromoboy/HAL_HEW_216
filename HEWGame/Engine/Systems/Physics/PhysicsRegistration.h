#pragma once

#include <functional>
#include <memory>
#include <unordered_set>
#include <vector>

#include "Core/UpdateNode.h"
#include "Systems/Physics/PhysicsMachine.h"

namespace Entities
{
    class PhysicalEntity;
}

namespace Systems
{
    class Box2DContactListener;
    class LateUpdateNode;

    class PhysicsRegistration
    {
    public:
        using ContactCallback = std::function<void(Entities::PhysicalEntity&, Entities::PhysicalEntity&)>;
        using ContactHitCallback = std::function<void(Entities::PhysicalEntity&, Entities::PhysicalEntity&, float)>;

        explicit PhysicsRegistration(PhysicsMachine& physicsMachine);
        ~PhysicsRegistration();

        void SetPlayer(Entities::PhysicalEntity* playerEntity);
        void RegisterPhysicsBody(Entities::PhysicalEntity& entity);
        void UnregisterPhysicsBody(Entities::PhysicalEntity* entity);
        void UnregisterAllPhysicsBodies();
        void ValidatePlayerPhysicsRegistration();
        void AddContactHandler(ContactCallback onBegin, ContactCallback onEnd, ContactCallback onLost, ContactHitCallback onHit);
        Core::IUpdatable* GetLateUpdateNode();
        const Core::IUpdatable* GetLateUpdateNode() const;

    private:
        struct ContactHandler
        {
            ContactCallback onBegin;
            ContactCallback onEnd;
            ContactCallback onLost;
            ContactHitCallback onHit;
        };

        void DispatchBeginContact(Entities::PhysicalEntity& a, Entities::PhysicalEntity& b);
        void DispatchEndContact(Entities::PhysicalEntity& a, Entities::PhysicalEntity& b);
        void DispatchLostContact(Entities::PhysicalEntity& a, Entities::PhysicalEntity& b);
        void DispatchHitContact(Entities::PhysicalEntity& a, Entities::PhysicalEntity& b, float approachSpeed);
        void FlushPendingUnregisters();

        PhysicsMachine* m_physicsMachine = nullptr;
        std::shared_ptr<Box2DContactListener> m_contactListener;
        std::unordered_set<Entities::PhysicalEntity*> m_registeredPhysicsBodies;
        std::unordered_set<Entities::PhysicalEntity*> m_pendingUnregisters;
        std::vector<ContactHandler> m_contactHandlers;
        Entities::PhysicalEntity* m_playerEntity = nullptr;
        bool m_loggedPlayerBodyRecovery = false;
        std::unique_ptr<LateUpdateNode> m_lateUpdateNode;

        friend class LateUpdateNode;
    };
}
