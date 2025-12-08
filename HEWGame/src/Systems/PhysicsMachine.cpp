#include "Systems/PhysicsMachine.h"
#include "Systems/IntegrateSystem.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>
#include <utility>

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
        PhysicsMachine::PhysicsMachine(float gravity)
                : Core::UpdateNode(Core::UpdatePhase::Physics, 0)
                , m_system(nullptr)
                , m_integrator(std::make_shared<IntegrateSystem>())
                , m_bodies()
                , m_bodyMap()
                , m_worldDef(b2DefaultWorldDef())
                , m_worldId(b2_nullWorldId)
                , m_loggedMissingSystem(false)
                , m_loggedMissingIntegrator(false)
                , m_loggedMissingWorld(false)
        {
                SetGravity(gravity);
                m_worldId = b2CreateWorld(&m_worldDef);
                if (!b2World_IsValid(m_worldId) && !m_loggedMissingWorld)
                {
                        DebugTrace("[PhysicsMachine] Failed to create Box2D world.\n");
                        m_loggedMissingWorld = true;
                }
        }

        PhysicsMachine::~PhysicsMachine()
        {
                ClearBodies();
                DestroyWorld();
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
                const float sanitized = std::isfinite(gravity) ? gravity : 9.8f;
                m_worldDef.gravity = b2Vec2{ 0.0f, -sanitized };
                if (b2World_IsValid(m_worldId))
                {
                        b2World_SetGravity(m_worldId, m_worldDef.gravity);
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
                return m_worldId;
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

                m_bodies.push_back(bodyPtr);

                b2BodyId bodyId = CreateBody(body);
                if (b2Body_IsValid(bodyId))
                {
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
                if (b2World_IsValid(m_worldId))
                {
                        constexpr int kSubStepCount = 4;
                        const float dt = std::max(0.0f, ctx.dt);
                        b2World_Step(m_worldId, dt, kSubStepCount);

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
                                        if (m_system)
                                        {
                                                m_system->Apply(*body, ctx.dt);
                                        }
                                        if (m_integrator)
                                        {
                                                m_integrator->Integrate(*body, ctx.dt);
                                        }
                                        body->SyncColliderBounds();
                                        continue;
                                }

                                const b2BodyId bodyId = mapIt->second;
                                const b2Vec2 position = b2Body_GetPosition(bodyId);
                                const b2Vec2 velocity = b2Body_GetLinearVelocity(bodyId);

                                Physics::Physical& physical = body->GetPhysical();
                                physical.velocity = float3{ velocity.x, velocity.y, 0.0f };
                                physical.force = float3{ 0.0f, 0.0f, 0.0f };

                                const auto& collisionEntity = static_cast<const Entities::CollisionEntity&>(*body);
                                Transform updatedTransform = collisionEntity.GetTransform();
                                updatedTransform.position.x = position.x;
                                updatedTransform.position.y = position.y;
                                body->SetTransform(updatedTransform);
                        }
                        return;
                }

                if (!m_loggedMissingWorld)
                {
                        DebugTrace("[PhysicsMachine] Box2D world is invalid; falling back to legacy integrator.\n");
                        m_loggedMissingWorld = true;
                }

                if (!m_system)
                {
                        if (!m_bodies.empty() && !m_loggedMissingSystem)
                        {
                                std::ostringstream oss;
                                oss << "[PhysicsMachine] No physics system configured while bodies are registered. bodyCount="
                                    << m_bodies.size() << "\n";
                                DebugTrace(oss.str());
                                m_loggedMissingSystem = true;
                        }
                        return;
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

                        m_system->Apply(*body, ctx.dt);

                        if (m_integrator)
                        {
                                m_integrator->Integrate(*body, ctx.dt);
                        }

                        body->SyncColliderBounds();
                }
        }

        b2BodyId PhysicsMachine::CreateBody(Entities::PhysicalEntity& body)
        {
                if (!b2World_IsValid(m_worldId))
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
                const AABB bounds = collisionEntity.GetCollider() ? collisionEntity.GetCollider()->GetAABB() : AABB{};
                const float halfWidth = std::max(0.05f, (bounds.max.x - bounds.min.x) * 0.5f);
                const float halfHeight = std::max(0.05f, (bounds.max.y - bounds.min.y) * 0.5f);

                b2BodyDef bodyDef = b2DefaultBodyDef();
                bodyDef.position = b2Vec2{ transform.position.x, transform.position.y };
                bodyDef.linearVelocity = b2Vec2{ body.GetPhysical().velocity.x, body.GetPhysical().velocity.y };
                bodyDef.type = (body.GetPhysical().weight > 0.0f) ? b2_dynamicBody : b2_staticBody;
                bodyDef.gravityScale = (bodyDef.type == b2_dynamicBody) ? 1.0f : 0.0f;

                const float damping = std::clamp(body.GetPhysical().damping, 0.0f, 1.0f);
                bodyDef.linearDamping = (damping > 0.0f) ? -std::log(damping) : 0.0f;
                bodyDef.userData = &body;

                b2BodyId bodyId = b2CreateBody(m_worldId, &bodyDef);
                if (!b2Body_IsValid(bodyId))
                {
                        DebugTrace("[PhysicsMachine] Failed to create Box2D body.\n");
                        return b2_nullBodyId;
                }

                b2ShapeDef shapeDef = b2DefaultShapeDef();
                shapeDef.density = bodyDef.type == b2_dynamicBody ? std::max(0.01f, body.GetPhysical().weight) : 0.0f;
                shapeDef.material.friction = 0.6f;

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
                        b2DestroyBody(it->second);
                }

                m_bodyMap.erase(it);
        }

        void PhysicsMachine::DestroyWorld()
        {
                if (b2World_IsValid(m_worldId))
                {
                        b2DestroyWorld(m_worldId);
                        m_worldId = b2_nullWorldId;
                }
        }
}

