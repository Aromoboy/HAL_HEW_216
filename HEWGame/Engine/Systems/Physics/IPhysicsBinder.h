#pragma once

#include <functional>
#include <optional>

#include <box2d/box2d.h>

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
        using BeginContactCallback = std::function<void(Entities::PhysicalEntity&, Entities::PhysicalEntity&, const std::optional<b2Vec2>&)>;
        using EndContactCallback = std::function<void(Entities::PhysicalEntity&, Entities::PhysicalEntity&)>;

        virtual ~IPhysicsBinder() = default;

        virtual PhysicsMachine* GetPhysicsMachine() const = 0;
        virtual void SetPlayer(Entities::PhysicalEntity* playerEntity) = 0;
        virtual void Enable(bool enabled) = 0;
        virtual void RegisterPhysicsBody(Entities::PhysicalEntity& entity) = 0;
        virtual void UnregisterPhysicsBody(Entities::PhysicalEntity* entity) = 0;
        virtual void UnregisterAllPhysicsBodies() = 0;
        virtual void ValidatePlayerPhysicsRegistration() = 0;
        virtual void AddContactHandler(BeginContactCallback onBegin, EndContactCallback onEnd) = 0;
    };
}
