#include "Systems/Box2DContactListener.h"

#include <span>
#include <sstream>

#include <box2d/collision.h>

#if defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

#include "Entities/PhysicalEntity.h"
#include "Systems/PhysicsMachine.h"

namespace
{
        void DebugTrace(const std::string& message)
        {
#if defined(_WIN32)
#if defined(_DEBUG)
                ::OutputDebugStringA(message.c_str());
#else
                (void)message;
#endif
#else
                (void)message;
#endif
        }
}

namespace Systems
{
        void Box2DContactListener::ProcessContacts()
        {
                if (!b2World_IsValid(m_worldId))
                {
                        return;
                }

                const b2ContactEvents events = b2World_GetContactEvents(m_worldId);

                // The event buffers (beginEvents/endEvents/hitEvents) are owned by Box2D and stay
                // valid until the next world step. They must not be freed by the listener.

                constexpr int kMaxLogsPerFrame = 4;
                int loggedMeshContacts = 0;

                const std::span<const b2ContactBeginTouchEvent> beginEvents =
                    (events.beginEvents && events.beginCount > 0)
                        ? std::span<const b2ContactBeginTouchEvent>(
                              events.beginEvents, static_cast<size_t>(events.beginCount))
                        : std::span<const b2ContactBeginTouchEvent>();
                for (const b2ContactBeginTouchEvent& evt : beginEvents)
                {
                        Entities::PhysicalEntity* entityA = GetEntityForShape(evt.shapeIdA);
                        Entities::PhysicalEntity* entityB = GetEntityForShape(evt.shapeIdB);
                        BeginContact(entityA, entityB);

                        if (m_logMeshContacts && loggedMeshContacts < kMaxLogsPerFrame)
                        {
                                const bool meshContact = (entityA && entityA->HasBox2DMesh())
                                        || (entityB && entityB->HasBox2DMesh());
                                if (meshContact)
                                {
                                        LogMeshContact(evt);
                                        ++loggedMeshContacts;
                                }
                        }
                }

                const std::span<const b2ContactEndTouchEvent> endEvents =
                    (events.endEvents && events.endCount > 0)
                        ? std::span<const b2ContactEndTouchEvent>(
                              events.endEvents, static_cast<size_t>(events.endCount))
                        : std::span<const b2ContactEndTouchEvent>();
                for (const b2ContactEndTouchEvent& evt : endEvents)
                {
                        Entities::PhysicalEntity* entityA = GetEntityForShape(evt.shapeIdA);
                        Entities::PhysicalEntity* entityB = GetEntityForShape(evt.shapeIdB);
                        EndContact(entityA, entityB);
                }

                // Contact hit events are intentionally ignored for now; Box2D owns the buffer.
                (void)events.hitEvents;
                (void)events.hitCount;

        }

        void Box2DContactListener::SetBeginContactCallback(ContactCallback callback)
        {
                m_onBeginContact = std::move(callback);
        }

        void Box2DContactListener::SetEndContactCallback(ContactCallback callback)
        {
                m_onEndContact = std::move(callback);
        }

        void Box2DContactListener::EnableMeshContactLogging(bool enabled)
        {
                m_logMeshContacts = enabled;
        }

        void Box2DContactListener::SetWorld(b2WorldId worldId)
        {
                m_worldId = worldId;
        }

        void Box2DContactListener::SetPhysicsMachine(PhysicsMachine* physicsMachine)
        {
                m_physicsMachine = physicsMachine;
        }

        void Box2DContactListener::BeginContact(Entities::PhysicalEntity* entityA, Entities::PhysicalEntity* entityB)
        {
                if (!entityA || !entityB)
                {
                        return;
                }

                if (m_onBeginContact)
                {
                        m_onBeginContact(*entityA, *entityB);
                }
        }

        void Box2DContactListener::EndContact(Entities::PhysicalEntity* entityA, Entities::PhysicalEntity* entityB)
        {
                if (!entityA || !entityB)
                {
                        return;
                }

                if (m_onEndContact)
                {
                        m_onEndContact(*entityA, *entityB);
                }
        }

        Entities::PhysicalEntity* Box2DContactListener::GetEntityForBody(b2BodyId bodyId) const
        {
                if (!b2Body_IsValid(bodyId))
                {
                        return nullptr;
                }

                return static_cast<Entities::PhysicalEntity*>(b2Body_GetUserData(bodyId));
        }

        Entities::PhysicalEntity* Box2DContactListener::GetEntityForShape(b2ShapeId shapeId) const
        {
                if (!b2Shape_IsValid(shapeId))
                {
                        return nullptr;
                }

                if (void* userData = b2Shape_GetUserData(shapeId))
                {
                        return static_cast<Entities::PhysicalEntity*>(userData);
                }

                const b2BodyId bodyId = b2Shape_GetBody(shapeId);
                if (Entities::PhysicalEntity* fromBody = GetEntityForBody(bodyId))
                {
                        return fromBody;
                }

                if (m_physicsMachine)
                {
                        const auto& bodies = m_physicsMachine->GetBodies();
                        for (Entities::PhysicalEntity* entity : bodies)
                        {
                                if (entity && B2_ID_EQUALS(entity->GetBox2DBodyId(), bodyId))
                                {
                                        return entity;
                                }
                        }
                }

                return nullptr;
        }

        void Box2DContactListener::LogMeshContact(const b2ContactBeginTouchEvent& evt) const
        {
                Entities::PhysicalEntity* entityA = GetEntityForBody(b2Shape_GetBody(evt.shapeIdA));
                Entities::PhysicalEntity* entityB = GetEntityForBody(b2Shape_GetBody(evt.shapeIdB));

                const char* tagA = entityA ? entityA->GetTag().c_str() : "<null>";
                const char* tagB = entityB ? entityB->GetTag().c_str() : "<null>";

                std::ostringstream oss;
                oss << "[Box2DContactListener] Mesh contact begin between '" << tagA << "' and '" << tagB << "'\n";
                DebugTrace(oss.str());
        }
}

