#include "Systems/Box2DContactListener.h"

#include <sstream>

#include <box2d/collision.h>

#if defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

#include "Entities/PhysicalEntity.h"

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
        void Box2DContactListener::ProcessContacts(b2WorldId worldId)
        {
                if (!b2World_IsValid(worldId))
                {
                        return;
                }

                const b2ContactEvents events = b2World_GetContactEvents(worldId);

                constexpr int kMaxLogsPerFrame = 4;
                int loggedMeshContacts = 0;

                for (int i = 0; i < events.beginCount; ++i)
                {
                        const b2ContactBeginTouchEvent& evt = events.beginEvents[i];
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

                for (int i = 0; i < events.endCount; ++i)
                {
                        const b2ContactEndTouchEvent& evt = events.endEvents[i];
                        Entities::PhysicalEntity* entityA = GetEntityForShape(evt.shapeIdA);
                        Entities::PhysicalEntity* entityB = GetEntityForShape(evt.shapeIdB);
                        EndContact(entityA, entityB);
                }

                if (events.beginEvents)
                {
                        b2Free(events.beginEvents);
                }
                if (events.endEvents)
                {
                        b2Free(events.endEvents);
                }
                if (events.hitEvents)
                {
                        b2Free(events.hitEvents);
                }
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

        Entities::PhysicalEntity* Box2DContactListener::GetEntityForShape(b2ShapeId shapeId) const
        {
                if (!b2Shape_IsValid(shapeId))
                {
                        return nullptr;
                }

                const b2BodyId bodyId = b2Shape_GetBody(shapeId);
                if (!b2Body_IsValid(bodyId))
                {
                        return nullptr;
                }

                return GetEntityForBody(bodyId);
        }

        Entities::PhysicalEntity* Box2DContactListener::GetEntityForBody(b2BodyId bodyId) const
        {
                if (!b2Body_IsValid(bodyId))
                {
                        return nullptr;
                }

                return static_cast<Entities::PhysicalEntity*>(b2Body_GetUserData(bodyId));
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

