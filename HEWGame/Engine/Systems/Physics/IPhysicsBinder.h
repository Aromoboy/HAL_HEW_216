#pragma once

#include <functional>

namespace Entities
{
    class PhysicalEntity;
}

namespace Core
{
    class IUpdatable;
}

namespace Systems
{
    class PhysicsMachine;

    class IPhysicsBinder
    {
    public:
        using ContactCallback = std::function<void(Entities::PhysicalEntity&, Entities::PhysicalEntity&)>;
        using ContactHitCallback = std::function<void(Entities::PhysicalEntity&, Entities::PhysicalEntity&, float)>;

        virtual ~IPhysicsBinder() = default;

        virtual PhysicsMachine* GetPhysicsMachine() const = 0;
        virtual void SetPlayer(Entities::PhysicalEntity* playerEntity) = 0;
        virtual void Enable(bool enabled) = 0;
        virtual void RegisterPhysicsBody(Entities::PhysicalEntity& entity) = 0;
        virtual void UnregisterPhysicsBody(Entities::PhysicalEntity* entity) = 0;
        virtual void UnregisterAllPhysicsBodies() = 0;
        virtual Core::IUpdatable* GetRegistrationLateUpdateNode() = 0;
        virtual void ValidatePlayerPhysicsRegistration() = 0;
        virtual void AddContactHandler(ContactCallback onBegin, ContactCallback onEnd, ContactCallback onLost,
            ContactHitCallback onHit) = 0;
    };
}
