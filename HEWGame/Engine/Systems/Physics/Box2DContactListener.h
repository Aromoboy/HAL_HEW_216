#pragma once

#include <functional>
#include <memory>
#include <optional>
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
                using BeginContactCallback = std::function<void(Entities::PhysicalEntity&, Entities::PhysicalEntity&, const std::optional<b2Vec2>&)>;
                using EndContactCallback = std::function<void(Entities::PhysicalEntity&, Entities::PhysicalEntity&)>;

                explicit Box2DContactListener(PhysicsMachine& physicsMachine);

                void ProcessContacts();

                void SetWorld(b2WorldId worldId);

                void SetBeginContactCallback(BeginContactCallback callback);
                void SetEndContactCallback(EndContactCallback callback);
                void EnableMeshContactLogging(bool enabled);
                void EnableDroppedEventLogging(bool enabled);
                void NotifyDestroyedBody(b2BodyId bodyId, std::span<const b2ShapeId> shapes = {});

        private:
                void BeginContact(Entities::PhysicalEntity* entityA, Entities::PhysicalEntity* entityB, const std::optional<b2Vec2>& contactPoint);
                void EndContact(Entities::PhysicalEntity* entityA, Entities::PhysicalEntity* entityB);
                Entities::PhysicalEntity* GetEntityForBody(b2BodyId bodyId) const;
                Entities::PhysicalEntity* GetEntityForShape(b2ShapeId shapeId) const;
                void LogMeshContact(const b2ContactBeginTouchEvent& evt) const;

                b2WorldId m_worldId{ b2_nullWorldId };
                BeginContactCallback m_onBeginContact;
                EndContactCallback m_onEndContact;
                bool m_logMeshContacts = false;
                bool m_logDroppedEvents = false;
                PhysicsMachine* m_physicsMachine = nullptr;
                std::unordered_set<uint64_t> m_destroyedBodyIds;
                std::unordered_set<uint64_t> m_destroyedShapeIds;
        };
}
