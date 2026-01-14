#pragma once

#include <functional>
#include <memory>
#include <span>
#include <unordered_set>

#include <box2d/box2d.h>

namespace Entities
{
        class PhysicalEntity;
}

namespace Systems
{
        class PhysicsMachine;

        class Box2DContactListener
        {
        public:
                using ContactCallback = std::function<void(Entities::PhysicalEntity&, Entities::PhysicalEntity&)>;
                using ContactHitCallback = std::function<void(Entities::PhysicalEntity&, Entities::PhysicalEntity&, float)>;

                explicit Box2DContactListener(PhysicsMachine& physicsMachine);

                void ProcessContacts();

                void SetWorld(b2WorldId worldId);

                void SetBeginContactCallback(ContactCallback callback);
                void SetEndContactCallback(ContactCallback callback);
                void SetHitContactCallback(ContactHitCallback callback);
                void EnableMeshContactLogging(bool enabled);
                void EnableDroppedEventLogging(bool enabled);
                void NotifyDestroyedBody(b2BodyId bodyId, std::span<const b2ShapeId> shapes = {});

        private:
                void BeginContact(Entities::PhysicalEntity* entityA, Entities::PhysicalEntity* entityB);
                void EndContact(Entities::PhysicalEntity* entityA, Entities::PhysicalEntity* entityB);
                void HitContact(Entities::PhysicalEntity* entityA, Entities::PhysicalEntity* entityB, float approachSpeed);
                Entities::PhysicalEntity* GetEntityForBody(b2BodyId bodyId) const;
                Entities::PhysicalEntity* GetEntityForShape(b2ShapeId shapeId) const;
                void LogMeshContact(const b2ContactBeginTouchEvent& evt) const;

                b2WorldId m_worldId{ b2_nullWorldId };
                ContactCallback m_onBeginContact;
                ContactCallback m_onEndContact;
                ContactHitCallback m_onHitContact;
                bool m_logMeshContacts = false;
                bool m_logDroppedEvents = false;
                PhysicsMachine* m_physicsMachine = nullptr;
                std::unordered_set<uint64_t> m_destroyedBodyIds;
                std::unordered_set<uint64_t> m_destroyedShapeIds;
        };
}
