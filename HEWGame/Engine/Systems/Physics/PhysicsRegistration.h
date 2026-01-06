#pragma once

#include <functional>
#include <memory>
#include <optional>
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
        using BeginContactCallback = std::function<void(Entities::PhysicalEntity&, Entities::PhysicalEntity&, const std::optional<b2Vec2>&)>;
        using EndContactCallback = std::function<void(Entities::PhysicalEntity&, Entities::PhysicalEntity&)>;

        explicit PhysicsRegistration(PhysicsMachine& physicsMachine);

        void SetPlayer(Entities::PhysicalEntity* playerEntity);
        void RegisterPhysicsBody(Entities::PhysicalEntity& entity);
        void UnregisterPhysicsBody(Entities::PhysicalEntity* entity);
        void UnregisterAllPhysicsBodies();
        void ValidatePlayerPhysicsRegistration();
        void AddContactHandler(BeginContactCallback onBegin, EndContactCallback onEnd);

    private:
        struct ContactHandler
        {
            BeginContactCallback onBegin;
            EndContactCallback onEnd;
        };

        void DispatchBeginContact(Entities::PhysicalEntity& a, Entities::PhysicalEntity& b, const std::optional<b2Vec2>& contactPoint);
        void DispatchEndContact(Entities::PhysicalEntity& a, Entities::PhysicalEntity& b);

        PhysicsMachine* m_physicsMachine = nullptr;
        std::shared_ptr<Box2DContactListener> m_contactListener;
        std::unordered_set<Entities::PhysicalEntity*> m_registeredPhysicsBodies;
        std::vector<ContactHandler> m_contactHandlers;
        Entities::PhysicalEntity* m_playerEntity = nullptr;
        bool m_loggedPlayerBodyRecovery = false;
    };
}
