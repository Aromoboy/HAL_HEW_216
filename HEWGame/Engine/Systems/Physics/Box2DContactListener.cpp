#include "Systems/Physics/Box2DContactListener.h"

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <box2d/collision.h>
#include <box2d/id.h>

#if defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

#include "Entities/PhysicalEntity.h"
#include "Systems/Physics/PhysicsMachine.h"

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
        Box2DContactListener::Box2DContactListener(PhysicsMachine& physicsMachine)
                : m_physicsMachine(&physicsMachine)
        {
        }

        void Box2DContactListener::CaptureContacts()
        {
                if (!b2World_IsValid(m_worldId))
                {
                        return;
                }

                const b2ContactEvents events = b2World_GetContactEvents(m_worldId);

                // ミカ：次のステップで失効するイベントと解決済み接触情報を値で保存する
                for (int i = 0; i < events.beginCount; ++i)
                {
                        const b2ContactBeginTouchEvent& event = events.beginEvents[i];
                        ContactEventInfo info{ event.shapeIdA, event.shapeIdB, event.contactId };
                        if (b2Contact_IsValid(event.contactId))
                        {
                                const b2ContactData data = b2Contact_GetData(event.contactId);
                                info.manifold = data.manifold;
                                info.hasManifold = info.manifold.pointCount > 0;
                                for (int point = 0; point < info.manifold.pointCount; ++point)
                                {
                                        const float impulse = info.manifold.points[point].totalNormalImpulse;
                                        if (std::isfinite(impulse))
                                        {
                                                info.normalImpulse += std::max(0.0f, impulse);
                                        }
                                }
                        }
                        m_pendingContacts.push_back({ info, true });
                }
                for (int i = 0; i < events.endCount; ++i)
                {
                        const b2ContactEndTouchEvent& event = events.endEvents[i];
                        m_pendingContacts.push_back({ { event.shapeIdA, event.shapeIdB, event.contactId }, false });
                }
        }

        void Box2DContactListener::ProcessContacts()
        {
                std::vector<PendingContact> pendingContacts;
                pendingContacts.swap(m_pendingContacts);
                const std::uint64_t eventEpoch = m_eventEpoch;
                if (!b2World_IsValid(m_worldId))
                {
                        ClearDestroyedCaches();
                        return;
                }

                constexpr int kMaxLogsPerFrame = 4;
                constexpr int kMaxDroppedEventLogsPerFrame = 8;
                int loggedMeshContacts = 0;
                int loggedDroppedEvents = 0;

                struct ShapeValidationResult
                {
                        bool valid{ false };
                        const char* reason{ nullptr };
                        uint64_t bodyKey{ 0 };
                };

                struct DroppedEventKey
                {
                        uint64_t bodyA = 0;
                        uint64_t bodyB = 0;
                        std::string eventType{};
                        std::string reason{};

                        bool operator==(const DroppedEventKey& other) const noexcept
                        {
                                return bodyA == other.bodyA
                                    && bodyB == other.bodyB
                                    && eventType == other.eventType
                                    && reason == other.reason;
                        }
                };

                struct DroppedEventKeyHash
                {
                        size_t operator()(const DroppedEventKey& key) const noexcept
                        {
                                size_t hash = std::hash<uint64_t>{}(key.bodyA);
                                hash ^= std::hash<uint64_t>{}(key.bodyB) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                                hash ^= std::hash<std::string>{}(key.eventType) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                                hash ^= std::hash<std::string>{}(key.reason) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                                return hash;
                        }
                };

                struct DroppedEventLog
                {
                        std::string message;
                        int count = 0;
                };

                std::unordered_map<DroppedEventKey, DroppedEventLog, DroppedEventKeyHash> droppedEventLogs;

                const auto validateShapeAndBody = [&](b2ShapeId shapeId)
                    {
                        ShapeValidationResult result{};
                        const uint64_t shapeKey = b2StoreShapeId(shapeId);
                        if (m_destroyedShapeIds.contains(shapeKey))
                        {
                                result.reason = "destroyed shape";
                                return result;
                        }

                        if (!b2Shape_IsValid(shapeId))
                        {
                                result.reason = "invalid shape";
                                return result;
                        }

                        const b2BodyId bodyId = b2Shape_GetBody(shapeId);
                        if (b2Body_IsValid(bodyId))
                        {
                                result.bodyKey = b2StoreBodyId(bodyId);
                        }

                        if (!b2Body_IsValid(bodyId))
                        {
                                result.reason = "invalid body";
                                return result;
                        }

                        if (m_destroyedBodyIds.contains(result.bodyKey))
                        {
                                result.reason = "destroyed body";
                                return result;
                        }

                        result.valid = true;
                        return result;
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

                const auto recordDroppedEvent = [&](const char* eventType, b2ShapeId shapeIdA, b2ShapeId shapeIdB,
                                                std::string reason, uint64_t bodyKeyA, uint64_t bodyKeyB)
                    {
                        if (!m_logDroppedEvents)
                        {
                                return;
                        }

                        const uint64_t orderedA = std::min(bodyKeyA, bodyKeyB);
                        const uint64_t orderedB = std::max(bodyKeyA, bodyKeyB);
                        DroppedEventKey key{ orderedA, orderedB, eventType ? eventType : "", std::move(reason) };
                        auto [it, inserted] = droppedEventLogs.try_emplace(std::move(key));
                        ++it->second.count;

                        if (inserted)
                        {
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
                                if (!it->first.reason.empty())
                                {
                                        oss << " reason=" << it->first.reason;
                                }

                                it->second.message = oss.str();
                        }
                    };

                const auto flushDroppedEventLogs = [&]()
                    {
                        for (const auto& [key, log] : droppedEventLogs)
                        {
                                if (loggedDroppedEvents >= kMaxDroppedEventLogsPerFrame)
                                {
                                        break;
                                }

                                std::ostringstream oss;
                                oss << log.message;
                                if (log.count > 1)
                                {
                                        oss << " occurrences=" << log.count;
                                }
                                oss << "\n";
                                DebugTrace(oss.str());
                                ++loggedDroppedEvents;
                        }
                    };

                // ミカ：ステップ順に配信し、先行コールバックが破棄したボディを毎回検証する
                for (const PendingContact& pending : pendingContacts)
                {
                        if (eventEpoch != m_eventEpoch)
                        {
                                break;
                        }
                        const ContactEventInfo& info = pending.info;
                        const ShapeValidationResult shapeAValidation = validateShapeAndBody(info.shapeIdA);
                        const ShapeValidationResult shapeBValidation = validateShapeAndBody(info.shapeIdB);
                        if (!shapeAValidation.valid || !shapeBValidation.valid)
                        {
                                const std::string reason = shapeAValidation.reason
                                    ? shapeAValidation.reason
                                    : (shapeBValidation.reason ? shapeBValidation.reason : "invalid shape or body");
                                recordDroppedEvent(pending.begin ? "begin" : "end", info.shapeIdA, info.shapeIdB, reason,
                                    shapeAValidation.bodyKey, shapeBValidation.bodyKey);
                                continue;
                        }

                        Entities::PhysicalEntity* entityA = GetEntityForShape(info.shapeIdA);
                        Entities::PhysicalEntity* entityB = GetEntityForShape(info.shapeIdB);

                        const bool entitiesValid = (entityA && entityB);
                        const bool bodiesMatchShapes = entitiesValid
                            && shapeMatchesEntity(entityA, info.shapeIdA)
                            && shapeMatchesEntity(entityB, info.shapeIdB);
                        const bool fixturesValid = bodiesMatchShapes && hasValidFixtures(entityA) && hasValidFixtures(entityB);

                        if (!fixturesValid)
                        {
                                continue;
                        }

                        if (pending.begin && m_logMeshContacts && loggedMeshContacts < kMaxLogsPerFrame)
                        {
                                const bool meshContact = (entityA && entityA->HasBox2DMesh())
                                        || (entityB && entityB->HasBox2DMesh());
                                if (meshContact)
                                {
                                        LogMeshContact({ info.shapeIdA, info.shapeIdB, info.contactId });
                                        ++loggedMeshContacts;
                                }
                        }
                        if (pending.begin)
                        {
                                BeginContact(entityA, entityB, info);
                        }
                        else
                        {
                                EndContact(entityA, entityB, info);
                        }
                }

                flushDroppedEventLogs();
                m_destroyedBodyIds.clear();
                m_destroyedShapeIds.clear();

                if (m_pendingContacts.empty())
                {
                        pendingContacts.clear();
                        pendingContacts.swap(m_pendingContacts);
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

        void Box2DContactListener::EnableDroppedEventLogging(bool enabled)
        {
                m_logDroppedEvents = enabled;
        }

        void Box2DContactListener::SetWorld(b2WorldId worldId)
        {
                ClearDestroyedCaches();
                m_worldId = worldId;
        }

        void Box2DContactListener::NotifyDestroyedBody(b2BodyId bodyId, std::span<const b2ShapeId> shapes)
        {
                if (b2Body_IsValid(bodyId))
                {
                        m_destroyedBodyIds.insert(b2StoreBodyId(bodyId));
                }

                for (const b2ShapeId shapeId : shapes)
                {
                        if (b2Shape_IsValid(shapeId))
                        {
                                m_destroyedShapeIds.insert(b2StoreShapeId(shapeId));
                        }
                }
        }

        void Box2DContactListener::ClearDestroyedCaches()
        {
                ++m_eventEpoch;
                m_pendingContacts.clear();
                m_destroyedBodyIds.clear();
                m_destroyedShapeIds.clear();
        }

        void Box2DContactListener::BeginContact(Entities::PhysicalEntity* entityA, Entities::PhysicalEntity* entityB,
            const ContactEventInfo& info)
        {
                if (!entityA || !entityB)
                {
                        return;
                }

                if (m_onBeginContact)
                {
                        m_onBeginContact(*entityA, *entityB, info);
                }
        }

        void Box2DContactListener::EndContact(Entities::PhysicalEntity* entityA, Entities::PhysicalEntity* entityB,
            const ContactEventInfo& info)
        {
                if (!entityA || !entityB)
                {
                        return;
                }

                if (m_onEndContact)
                {
                        m_onEndContact(*entityA, *entityB, info);
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
