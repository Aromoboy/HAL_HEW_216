#include "Systems/Box2DContactListener.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <sstream>

#include <box2d/collision.h>
#include <box2d/id.h>

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
                constexpr int kMaxDroppedEventLogsPerFrame = 8;
                int loggedMeshContacts = 0;
                int loggedDroppedEvents = 0;

                const auto validateShapeAndBody = [&](b2ShapeId shapeId)
                    {
                        return b2Shape_IsValid(shapeId) && b2Body_IsValid(b2Shape_GetBody(shapeId));
                    };

                const auto shapeMatchesEntity = [&](Entities::PhysicalEntity* entity, b2ShapeId shapeId)
                    {
                        if (!entity)
                        {
                            return false;
                        }

                        if (!b2Shape_IsValid(shapeId))
                        {
                            return false;
                        }

                        const b2BodyId shapeBodyId = b2Shape_GetBody(shapeId);
                        const b2BodyId entityBodyId = entity->GetBox2DBodyId();
                        if (!b2Body_IsValid(shapeBodyId) || !b2Body_IsValid(entityBodyId))
                        {
                            return false;
                        }

                        return entity->HasBox2DBody()
                            && b2StoreBodyId(shapeBodyId) == b2StoreBodyId(entityBodyId);
                    };

                const auto hasValidFixtures = [](Entities::PhysicalEntity* entity)
                    {
                        if (!entity)
                        {
                            return false;
                        }

                        const auto& colliderComponent = entity->GetBoxColliderComponent();
                        const int createdFixtures = colliderComponent ? colliderComponent->createdFixtureCount : 0;
                        return createdFixtures > 0;
                    };

                const auto logDroppedEvent = [&](const char* eventType, b2ShapeId shapeIdA, b2ShapeId shapeIdB,
                                                 const char* reason)
                    {
                        if (loggedDroppedEvents >= kMaxDroppedEventLogsPerFrame)
                        {
                            return;
                        }

                        std::ostringstream oss;
                        oss << "[Box2DContactListener][Error] Dropped " << eventType
                            << " contact event for shapes=";
                        if (b2Shape_IsValid(shapeIdA))
                        {
                                oss << b2StoreShapeId(shapeIdA);
                        }
                        else
                        {
                                oss << "<invalid>";
                        }
                        oss << ",";
                        if (b2Shape_IsValid(shapeIdB))
                        {
                                oss << b2StoreShapeId(shapeIdB);
                        }
                        else
                        {
                                oss << "<invalid>";
                        }
                        if (reason)
                        {
                                oss << " reason=" << reason;
                        }
                        oss << "\n";

                        DebugTrace(oss.str());
                        ++loggedDroppedEvents;
                    };

                const std::span<const b2ContactBeginTouchEvent> beginEvents =
                    (events.beginEvents && events.beginCount > 0)
                        ? std::span<const b2ContactBeginTouchEvent>(
                              events.beginEvents, static_cast<size_t>(events.beginCount))
                        : std::span<const b2ContactBeginTouchEvent>();
                for (const b2ContactBeginTouchEvent& evt : beginEvents)
                {
                        const bool shapeAValid = validateShapeAndBody(evt.shapeIdA);
                        const bool shapeBValid = validateShapeAndBody(evt.shapeIdB);
                        if (!shapeAValid || !shapeBValid)
                        {
                                continue;
                        }

                        Entities::PhysicalEntity* entityA = GetEntityForShape(evt.shapeIdA);
                        Entities::PhysicalEntity* entityB = GetEntityForShape(evt.shapeIdB);

                        const bool entitiesValid = (entityA && entityB);
                        const bool bodiesMatchShapes = entitiesValid
                            && shapeMatchesEntity(entityA, evt.shapeIdA)
                            && shapeMatchesEntity(entityB, evt.shapeIdB);
                        const bool fixturesValid = bodiesMatchShapes && hasValidFixtures(entityA) && hasValidFixtures(entityB);

                        if (!fixturesValid)
                        {
                                if (bodiesMatchShapes)
                                {
                                        logDroppedEvent("begin", evt.shapeIdA, evt.shapeIdB,
                                            "missing body or fixtures");
                                }
                                continue;
                        }

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
                        const bool shapeAValid = validateShapeAndBody(evt.shapeIdA);
                        const bool shapeBValid = validateShapeAndBody(evt.shapeIdB);
                        if (!shapeAValid || !shapeBValid)
                        {
                                continue;
                        }

                        Entities::PhysicalEntity* entityA = GetEntityForShape(evt.shapeIdA);
                        Entities::PhysicalEntity* entityB = GetEntityForShape(evt.shapeIdB);

                        const bool entitiesValid = (entityA && entityB);
                        const bool bodiesMatchShapes = entitiesValid
                            && shapeMatchesEntity(entityA, evt.shapeIdA)
                            && shapeMatchesEntity(entityB, evt.shapeIdB);
                        const bool fixturesValid = bodiesMatchShapes && hasValidFixtures(entityA) && hasValidFixtures(entityB);

                        if (!fixturesValid)
                        {
                                if (bodiesMatchShapes)
                                {
                                        logDroppedEvent("end", evt.shapeIdA, evt.shapeIdB,
                                            "missing body or fixtures");
                                }
                                continue;
                        }

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
                if (!b2Body_IsValid(bodyId))
                {
                        return nullptr;
                }

                if (Entities::PhysicalEntity* fromBody = GetEntityForBody(bodyId))
                {
                        return fromBody;
                }

                if (m_physicsMachine)
                {
                        const uint64_t targetKey = b2StoreBodyId(bodyId);
                        const auto& bodies = m_physicsMachine->GetBodies();
                        for (Entities::PhysicalEntity* entity : bodies)
                        {
                                if (!entity)
                                {
                                        continue;
                                }

                                const b2BodyId entityBodyId = entity->GetBox2DBodyId();
                                if (b2Body_IsValid(entityBodyId)
                                        && b2StoreBodyId(entityBodyId) == targetKey)
                                {
                                        return entity;
                                }
                        }
                }

                return nullptr;
        }

        void Box2DContactListener::LogMeshContact(const b2ContactBeginTouchEvent& evt) const
        {
                if (!b2Shape_IsValid(evt.shapeIdA) || !b2Shape_IsValid(evt.shapeIdB))
                {
                        return;
                }

                const b2BodyId bodyIdA = b2Shape_GetBody(evt.shapeIdA);
                const b2BodyId bodyIdB = b2Shape_GetBody(evt.shapeIdB);
                if (!b2Body_IsValid(bodyIdA) || !b2Body_IsValid(bodyIdB))
                {
                        return;
                }

                Entities::PhysicalEntity* entityA = GetEntityForBody(bodyIdA);
                Entities::PhysicalEntity* entityB = GetEntityForBody(bodyIdB);

                const char* tagA = entityA ? entityA->GetTag().c_str() : "<null>";
                const char* tagB = entityB ? entityB->GetTag().c_str() : "<null>";

                const std::size_t polygonCountA = entityA ? entityA->GetBox2DPolygons().size() : 0u;
                const std::size_t boundaryCountA = entityA ? entityA->GetBox2DBoundary().size() : 0u;
                const std::size_t polygonCountB = entityB ? entityB->GetBox2DPolygons().size() : 0u;
                const std::size_t boundaryCountB = entityB ? entityB->GetBox2DBoundary().size() : 0u;

                std::ostringstream oss;
                oss << "[Box2DContactListener] Mesh contact begin between '" << tagA << "' (polygons="
                    << polygonCountA << " boundary=" << boundaryCountA << ") and '" << tagB << "' (polygons="
                    << polygonCountB << " boundary=" << boundaryCountB << ")\n";
                DebugTrace(oss.str());
        }
}

