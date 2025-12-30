#pragma once

#include <functional>
#include <memory>
#include <unordered_set>
#include <vector>

#include "Entities/PhysicalEntity.h"
#include "Systems/PhysicsMachine.h"

namespace Entities
{
    class Player;
}

namespace Systems
{
    class Box2DContactListener;

    class PhysicsBinder
    {
    public:
        using ContactCallback = std::function<void(Entities::PhysicalEntity&, Entities::PhysicalEntity&)>;

        explicit PhysicsBinder(const PhysicsTuningParameters& tuning = {});

        PhysicsMachine* GetPhysicsMachine() const;
        void SetPlayer(Entities::PhysicalEntity* playerEntity);
        void Enable(bool enabled);

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

        std::unique_ptr<PhysicsMachine> m_physicsMachine;
        std::shared_ptr<Box2DContactListener> m_contactListener;
        std::unordered_set<Entities::PhysicalEntity*> m_registeredPhysicsBodies;
        std::vector<ContactHandler> m_contactHandlers;
        Entities::PhysicalEntity* m_playerEntity = nullptr;
        bool m_loggedPlayerBodyRecovery = false;
    };
}
