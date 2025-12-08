#pragma once

#include <functional>
#include <memory>

#include <box2d/box2d.h>

namespace Entities
{
        class PhysicalEntity;
}

namespace Systems
{
        class Box2DContactListener
        {
        public:
                using ContactCallback = std::function<void(Entities::PhysicalEntity&, Entities::PhysicalEntity&)>;

                Box2DContactListener() = default;

                void ProcessContacts(b2WorldId worldId);

                void SetBeginContactCallback(ContactCallback callback);
                void SetEndContactCallback(ContactCallback callback);
                void EnableMeshContactLogging(bool enabled);

        private:
                void BeginContact(Entities::PhysicalEntity* entityA, Entities::PhysicalEntity* entityB);
                void EndContact(Entities::PhysicalEntity* entityA, Entities::PhysicalEntity* entityB);
                Entities::PhysicalEntity* GetEntityForBody(b2BodyId bodyId) const;
                void LogMeshContact(const b2ContactBeginTouchEvent& evt) const;

                ContactCallback m_onBeginContact;
                ContactCallback m_onEndContact;
                bool m_logMeshContacts = false;
        };
}

