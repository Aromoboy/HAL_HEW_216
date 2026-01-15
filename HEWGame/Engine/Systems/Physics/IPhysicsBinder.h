#pragma once

#include <functional>

#include "Systems/Physics/ContactEventInfo.h"

namespace Entities
{
    class PhysicalEntity;
}

namespace Systems
{
    class PhysicsMachine;

    class IPhysicsBinder
    {
    public:
        using ContactCallback = std::function<void(Entities::PhysicalEntity&, Entities::PhysicalEntity&, const ContactEventInfo&)>;

        virtual ~IPhysicsBinder() = default;

        virtual PhysicsMachine* GetPhysicsMachine() const = 0;
        virtual void SetPlayer(Entities::PhysicalEntity* playerEntity) = 0;
        virtual void Enable(bool enabled) = 0;
        virtual void RegisterPhysicsBody(Entities::PhysicalEntity& entity) = 0;
        virtual void UnregisterPhysicsBody(Entities::PhysicalEntity* entity) = 0;
        virtual void UnregisterAllPhysicsBodies() = 0;
        virtual void ValidatePlayerPhysicsRegistration() = 0;
        virtual void AddContactHandler(ContactCallback onBegin, ContactCallback onEnd) = 0;
    };
}
