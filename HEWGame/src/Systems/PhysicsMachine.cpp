#include "Systems/PhysicsMachine.h"
#include "Systems/Box2DPhysicsSystem.h"
#include "Systems/IntegrateSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>

#include <box2d/box2d.h>
#include <box2d/collision.h>
#include <box2d/math_functions.h>

#if defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

#include "Core/IUpdatable.h"

namespace
{
        constexpr uint16_t kDefaultCategory = 0x0001;
        constexpr uint16_t kPlayerCategory = 0x0002;
        constexpr uint16_t kFloorCategory = 0x0004;
        constexpr uint16_t kSweetsCategory = 0x0008;

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

        uint16_t ToCategoryBits(Collision::CollisionLayer layer)
        {
                switch (layer)
                {
                case Collision::CollisionLayer::Player:
                        return kPlayerCategory;
                case Collision::CollisionLayer::Floor:
                        return kFloorCategory;
                case Collision::CollisionLayer::Sweets:
                        return kSweetsCategory;
                default:
                        return kDefaultCategory;
                }
        }

        uint16_t ToMaskBits(Collision::CollisionLayer layer)
        {
                switch (layer)
                {
                case Collision::CollisionLayer::Player:
                        return kDefaultCategory | kFloorCategory | kSweetsCategory | kPlayerCategory;
                case Collision::CollisionLayer::Floor:
                        return kDefaultCategory | kPlayerCategory | kSweetsCategory;
                case Collision::CollisionLayer::Sweets:
                        return kDefaultCategory | kFloorCategory | kPlayerCategory | kSweetsCategory;
                default:
                        return kDefaultCategory | kPlayerCategory | kFloorCategory | kSweetsCategory;
                }
        }
}

namespace Systems
{
        PhysicsMachine::PhysicsMachine(float gravity)
                : Core::UpdateNode(Core::UpdatePhase::Physics, 0)
                , m_box2dSystem(std::make_shared<Box2DPhysicsSystem>(gravity))
                , m_system(m_box2dSystem)
                , m_integrator(std::make_shared<IntegrateSystem>())
                , m_bodies()
                , m_bodyMap()
                , m_loggedMissingSystem(false)
                , m_loggedMissingIntegrator(false)
                , m_loggedMissingWorld(false)
        {
        }

        PhysicsMachine::~PhysicsMachine()
        {
                ClearBodies();
        }

        void PhysicsMachine::SetSystem(SystemPtr system)
        {
                if (m_system == system)
                {
                        return;
                }

#if defined(_DEBUG)
                {
                        std::ostringstream oss;
                        oss << "[PhysicsMachine] SetSystem called. previous=" << m_system.get()
                            << " new=" << (system ? system.get() : nullptr) << "\n";
                        DebugTrace(oss.str());
                }
#endif

                m_system = std::move(system);
                m_box2dSystem = std::dynamic_pointer_cast<Box2DPhysicsSystem>(m_system);
                m_loggedMissingSystem = false;
        }

        void PhysicsMachine::SetIntegrator(IntegratorPtr integrator)
        {
                if (m_integrator == integrator)
                {
                        return;
                }

#if defined(_DEBUG)
                {
                        std::ostringstream oss;
                        oss << "[PhysicsMachine] SetIntegrator called. previous=" << m_integrator.get()
                            << " new=" << (integrator ? integrator.get() : nullptr) << "\n";
                        DebugTrace(oss.str());
                }
#endif

                m_integrator = std::move(integrator);
                m_loggedMissingIntegrator = false;
        }

        PhysicsMachine::SystemPtr PhysicsMachine::GetSystem() const
        {
                return m_system;
        }

        PhysicsMachine::IntegratorPtr PhysicsMachine::GetIntegrator() const
        {
                return m_integrator;
        }

        const std::vector<Entities::PhysicalEntity*>& PhysicsMachine::GetBodies() const
        {
                return m_bodies;
        }

        void PhysicsMachine::SetGravity(float gravity)
        {
                if (m_box2dSystem)
                {
                        m_box2dSystem->SetGravity(gravity);
                        m_loggedMissingWorld = false;
                }
        }

        b2BodyId PhysicsMachine::FindBody(const Entities::PhysicalEntity& body) const
        {
                const auto it = m_bodyMap.find(const_cast<Entities::PhysicalEntity*>(&body));
                if (it != m_bodyMap.end())
                {
                        return it->second;
                }
                return b2_nullBodyId;
        }

        b2WorldId PhysicsMachine::GetWorldId() const
        {
                return m_box2dSystem ? m_box2dSystem->GetWorldId() : b2_nullWorldId;
        }

        void PhysicsMachine::RegisterBody(Entities::PhysicalEntity& body)
        {
                auto* bodyPtr = &body;
                if (std::find(m_bodies.begin(), m_bodies.end(), bodyPtr) != m_bodies.end())
                {
#if defined(_DEBUG)
                        std::ostringstream oss;
                        oss << "[PhysicsMachine] Duplicate body registration detected body=" << bodyPtr << "\n";
                        DebugTrace(oss.str());
#endif
                        return;
                }

                const b2BodyId existing = body.GetBox2DBodyId();
                if (b2Body_IsValid(existing))
                {
                        if (b2Body_GetWorld(existing).index == m_worldId.index)
                        {
                                m_bodies.push_back(bodyPtr);
                                m_bodyMap.emplace(bodyPtr, existing);
#if defined(_DEBUG)
                                std::ostringstream oss;
                                oss << "[PhysicsMachine] Registered pre-created body for tag='" << body.GetTag() << "' id="
                                    << existing.index << "\n";
                                DebugTrace(oss.str());
#endif
                                return;
                        }

#if defined(_DEBUG)
                        std::ostringstream oss;
                        oss << "[PhysicsMachine] Ignored foreign Box2D body for tag='" << body.GetTag()
                            << "' worldIndex=" << b2Body_GetWorld(existing).index
                            << " expectedWorld=" << m_worldId.index << "\n";
                        DebugTrace(oss.str());
#endif
                        body.SetBox2DBodyId(b2_nullBodyId);
                }

                m_bodies.push_back(bodyPtr);

                b2BodyId bodyId = CreateBody(body);
                if (b2Body_IsValid(bodyId))
                {
                        body.SetBox2DBodyId(bodyId);
                        m_bodyMap.emplace(bodyPtr, bodyId);
                }
        }

        void PhysicsMachine::UnregisterBody(Entities::PhysicalEntity& body)
        {
                auto* bodyPtr = &body;
                auto it = std::remove(m_bodies.begin(), m_bodies.end(), bodyPtr);
                if (it == m_bodies.end())
                {
#if defined(_DEBUG)
                        std::ostringstream oss;
                        oss << "[PhysicsMachine] Unregister requested for unknown body body=" << bodyPtr << "\n";
                        DebugTrace(oss.str());
#endif
                        DestroyBody(body);
                        return;
                }

                m_bodies.erase(it, m_bodies.end());
                DestroyBody(body);
        }

        void PhysicsMachine::ClearBodies()
        {
                for (Entities::PhysicalEntity* body : m_bodies)
                {
                        if (body)
                        {
                                DestroyBody(*body);
                        }
                }
                m_bodyMap.clear();
                m_bodies.clear();
        }

        void PhysicsMachine::OnUpdate(Core::UpdateContext& ctx)
        {
                const b2WorldId worldId = GetWorldId();

                if (!m_system)
                {
                        if (!m_loggedMissingSystem)
                        {
                                DebugTrace("[PhysicsMachine] No physics system configured; skipping step.\n");
                                m_loggedMissingSystem = true;
                        }
                        return;
                }

                if (b2World_IsValid(worldId))
                {
                        m_system->Apply(ctx.dt);

                        LogMeshContacts();

                        for (Entities::PhysicalEntity* body : m_bodies)
                        {
                                if (!body)
                                {
#if defined(_DEBUG)
                                        DebugTrace("[PhysicsMachine] Encountered null body pointer during update.\n");
#endif
                                        continue;
                                }

                                auto mapIt = m_bodyMap.find(body);
                                const bool hasBody = (mapIt != m_bodyMap.end()) && b2Body_IsValid(mapIt->second);
                                if (!hasBody)
                                {
                                        if (m_integrator)
                                        {
                                                m_integrator->Integrate(*body, ctx.dt);
                                        }
                                        body->SyncColliderBounds();
                                        continue;
                                }

                                const b2BodyId bodyId = mapIt->second;
                                const b2Vec2 position = b2Body_GetPosition(bodyId);
                                const float angle = b2Body_GetAngle(bodyId);

                                Transform updated = body->GetTransform();
                                updated.position = Entities::PhysicalEntity::ToWorldPosition(position);
                                updated.rotation.z = Entities::PhysicalEntity::ToWorldAngle(angle);

                                body->SetTransform(updated);
                                body->SetBox2DBodyId(bodyId);
                                body->SyncColliderBounds();
                        }
                        return;
                }

                if (!m_loggedMissingWorld)
                {
                        DebugTrace("[PhysicsMachine] Box2D world is invalid; falling back to legacy integrator.\n");
                        m_loggedMissingWorld = true;
                }

                if (!m_integrator && !m_loggedMissingIntegrator)
                {
                        DebugTrace("[PhysicsMachine] No integrator configured; skipping translation step.\n");
                        m_loggedMissingIntegrator = true;
                }

                for (Entities::PhysicalEntity* body : m_bodies)
                {
                        if (!body)
                        {
#if defined(_DEBUG)
                                DebugTrace("[PhysicsMachine] Encountered null body pointer during update.\n");
#endif
                                continue;
                        }

                        if (m_integrator)
                        {
                                m_integrator->Integrate(*body, ctx.dt);
                        }

                        body->SyncColliderBounds();
                }
        }

        b2BodyId PhysicsMachine::CreateBody(Entities::PhysicalEntity& body)
        {
                const b2WorldId worldId = GetWorldId();

                if (!b2World_IsValid(worldId))
                {
                        if (!m_loggedMissingWorld)
                        {
                                DebugTrace("[PhysicsMachine] Cannot create body; Box2D world is invalid.\n");
                                m_loggedMissingWorld = true;
                        }
                        return b2_nullBodyId;
                }

                body.SyncColliderBounds();
                const auto& collisionEntity = static_cast<const Entities::CollisionEntity&>(body);
                const Transform& transform = collisionEntity.GetTransform();
                const auto colliderComponent = body.GetBoxColliderComponent();
                const bool hasColliderComponent = colliderComponent && !colliderComponent->Empty();
                const auto shapeConfig = body.GetBox2DShapeConfig();
                const bool hasShapeConfig = shapeConfig && shapeConfig->type != Physics::Box2DShapeConfig::Type::None;
                const AABB bounds = collisionEntity.GetCollider() ? collisionEntity.GetCollider()->GetAABB() : AABB{};
                const float halfWidth = Entities::PhysicalEntity::ToBox2DLength(std::max(0.05f, (bounds.max.x - bounds.min.x) * 0.5f));
                const float halfHeight = Entities::PhysicalEntity::ToBox2DLength(std::max(0.05f, (bounds.max.y - bounds.min.y) * 0.5f));

                const auto& meshPolygons = body.GetBox2DPolygons();
                const auto& meshBoundary = body.GetBox2DBoundary();
                const bool hasMeshFixtures = !meshPolygons.empty();

                b2BodyDef bodyDef = b2DefaultBodyDef();
                bodyDef.position = Entities::PhysicalEntity::ToBox2DPosition(transform.position);
                bodyDef.type = hasShapeConfig ? shapeConfig->bodyType : b2_dynamicBody;
                bodyDef.angle = Entities::PhysicalEntity::ToBox2DAngle(transform.rotation.z);
                // 初期速度は shapeConfig が指定していれば使用、無ければ 0
                bodyDef.linearVelocity =
                    (hasShapeConfig && shapeConfig->initialVelocity.has_value())
                        ? Entities::PhysicalEntity::ToBox2DVector(shapeConfig->initialVelocity.value())
                        : b2Vec2{0.0f, 0.0f};
                // 重力スケール（動的ボディのみ適用）
                bodyDef.gravityScale = (bodyDef.type == b2_dynamicBody) ? 1.0f : 0.0f;
                // 減衰（shapeConfig 優先）
                bodyDef.linearDamping = hasShapeConfig ? shapeConfig->linearDamping : 0.0f;
                bodyDef.userData = &body;

                b2BodyId bodyId = b2CreateBody(worldId, &bodyDef);
                if (!b2Body_IsValid(bodyId))
                {
                        DebugTrace("[PhysicsMachine] Failed to create Box2D body.\n");
                        return b2_nullBodyId;
                }

                b2ShapeDef shapeDef = b2DefaultShapeDef();
                if (hasColliderComponent)
                {
                        shapeDef = colliderComponent->shapeDef;
                }

                const float configuredDensity = hasShapeConfig
                        ? std::max(0.0f, shapeConfig->density)
                        : 1.0f;
                shapeDef.density = bodyDef.type == b2_dynamicBody ? configuredDensity : 0.0f;
                shapeDef.material.friction = hasShapeConfig ? shapeConfig->friction : 0.6f;
                shapeDef.material.restitution = hasShapeConfig ? shapeConfig->restitution : shapeDef.material.restitution;
                shapeDef.enableContactEvents = true;
                shapeDef.enableHitEvents = true;
                shapeDef.filter.categoryBits = ToCategoryBits(layer);
                shapeDef.filter.maskBits = ToMaskBits(layer);

                if (hasColliderComponent)
                {
                        for (const auto& polygon : colliderComponent->polygons)
                        {
                                b2CreatePolygonShape(bodyId, &shapeDef, &polygon);
                        }

                        for (const auto& circle : colliderComponent->circles)
                        {
                                b2CreateCircleShape(bodyId, &shapeDef, &circle);
                        }

                        if (!colliderComponent->Empty())
                        {
                                return bodyId;
                        }
                }

                if (hasShapeConfig)
                {
                        switch (shapeConfig->type)
                        {
                        case Physics::Box2DShapeConfig::Type::Box:
                        {
                                const float hx = Entities::PhysicalEntity::ToBox2DLength(std::max(0.01f, 0.5f * transform.scale.x));
                                const float hy = Entities::PhysicalEntity::ToBox2DLength(std::max(0.01f, 0.5f * transform.scale.y));
                                const b2Polygon box = b2MakeBox(hx, hy);
                                b2CreatePolygonShape(bodyId, &shapeDef, &box);
                                return bodyId;
                        }
                        case Physics::Box2DShapeConfig::Type::Polygon:
                        {
                                std::vector<b2Vec2> scaled = shapeConfig->vertices;
                                for (auto& v : scaled)
                                {
                                        v.x = Entities::PhysicalEntity::ToBox2DLength(v.x * transform.scale.x);
                                        v.y = Entities::PhysicalEntity::ToBox2DLength(v.y * transform.scale.y);
                                }

                                if (scaled.size() >= 3)
                                {
                                        const b2Hull hull = b2ComputeHull(scaled.data(), static_cast<int>(scaled.size()));
                                        if (b2ValidateHull(&hull))
                                        {
                                                const b2Polygon shape = b2MakePolygon(&hull, b2_polygonRadius);
                                                b2CreatePolygonShape(bodyId, &shapeDef, &shape);
                                                return bodyId;
                                        }
                                }

                                break;
                        }
                        case Physics::Box2DShapeConfig::Type::Circle:
                        {
                                const float radius = std::max(0.01f, shapeConfig->radius);
                                const float scaledRadius = Entities::PhysicalEntity::ToBox2DLength(radius * std::max(transform.scale.x, transform.scale.y));
                                const b2Circle circle{ b2Vec2{0.0f, 0.0f}, scaledRadius };
                                b2CreateCircleShape(bodyId, &shapeDef, &circle);
                                return bodyId;
                        }
                        default:
                                break;
                        }
                }

                int meshShapes = 0;
                if (hasMeshFixtures)
                {
                        for (const auto& polygon : meshPolygons)
                        {
                                if (polygon.size() < 3)
                                {
                                        continue;
                                }

                                std::vector<b2Vec2> scaled = polygon;
                                for (auto& v : scaled)
                                {
                                        v.x = Entities::PhysicalEntity::ToBox2DLength(v.x * transform.scale.x);
                                        v.y = Entities::PhysicalEntity::ToBox2DLength(v.y * transform.scale.y);
                                }

                                const b2Hull hull = b2ComputeHull(scaled.data(), static_cast<int>(scaled.size()));
                                if (!b2ValidateHull(&hull))
                                {
                                        continue;
                                }

                                const b2Polygon shape = b2MakePolygon(&hull, b2_polygonRadius);
                                b2CreatePolygonShape(bodyId, &shapeDef, &shape);
                                ++meshShapes;
                        }

                        if (meshBoundary.size() >= 2)
                        {
                                std::vector<b2Vec2> scaledBoundary = meshBoundary;
                                for (auto& v : scaledBoundary)
                                {
                                        v.x = Entities::PhysicalEntity::ToBox2DLength(v.x * transform.scale.x);
                                        v.y = Entities::PhysicalEntity::ToBox2DLength(v.y * transform.scale.y);
                                }

                                b2ChainDef chainDef = b2DefaultChainDef();
                                chainDef.points = scaledBoundary.data();
                                chainDef.count = static_cast<int>(scaledBoundary.size());
                                chainDef.loop = true;

                                b2ShapeDef chainShapeDef = shapeDef;
                                chainShapeDef.density = 0.0f;
                                b2CreateChainShape(bodyId, &chainShapeDef, &chainDef);
                                ++meshShapes;
                        }

                        if (meshShapes > 0)
                        {
#if defined(_DEBUG)
                                std::ostringstream oss;
                                oss << "[PhysicsMachine] Created mesh fixtures for body tag='" << body.GetTag()
                                    << "' polygons=" << meshPolygons.size()
                                    << " boundaryPoints=" << meshBoundary.size() << "\n";
                                DebugTrace(oss.str());
#endif
                                return bodyId;
                        }
                }

                const b2Polygon box = b2MakeBox(halfWidth, halfHeight);
                b2CreatePolygonShape(bodyId, &shapeDef, &box);

                return bodyId;
        }

        void PhysicsMachine::DestroyBody(const Entities::PhysicalEntity& body)
        {
                auto it = m_bodyMap.find(const_cast<Entities::PhysicalEntity*>(&body));
                if (it == m_bodyMap.end())
                {
                        return;
                }

                if (b2Body_IsValid(it->second))
                {
                        const b2WorldId bodyWorld = b2Body_GetWorld(it->second);
                        if (b2World_IsValid(bodyWorld))
                        {
                                b2DestroyBody(it->second);
                        }
                }

                const_cast<Entities::PhysicalEntity&>(body).SetBox2DBodyId(b2_nullBodyId);
                m_bodyMap.erase(it);
        }

        void PhysicsMachine::LogMeshContacts()
        {
                const b2WorldId worldId = GetWorldId();

                if (!m_logMeshContacts || !b2World_IsValid(worldId))
                {
                        return;
                }

                const b2ContactEvents events = b2World_GetContactEvents(worldId);
                constexpr int kMaxLogsPerFrame = 4;
                int logged = 0;

                for (int i = 0; i < events.beginCount && logged < kMaxLogsPerFrame; ++i)
                {
                        const b2ContactBeginTouchEvent& evt = events.beginEvents[i];
                        Entities::PhysicalEntity* bodyA = static_cast<Entities::PhysicalEntity*>(b2Body_GetUserData(evt.bodyIdA));
                        Entities::PhysicalEntity* bodyB = static_cast<Entities::PhysicalEntity*>(b2Body_GetUserData(evt.bodyIdB));

                        const bool meshContact = (bodyA && bodyA->HasBox2DMesh()) || (bodyB && bodyB->HasBox2DMesh());
                        if (!meshContact)
                        {
                                continue;
                        }

                        const char* tagA = bodyA ? bodyA->GetTag().c_str() : "<null>";
                        const char* tagB = bodyB ? bodyB->GetTag().c_str() : "<null>";

                        std::ostringstream oss;
                        oss << "[PhysicsMachine] Mesh contact begin between '" << tagA << "' and '" << tagB << "'\n";
                        DebugTrace(oss.str());
                        ++logged;
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
}

