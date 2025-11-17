#include "Systems/ContactLiteSystem.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

#if defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

#include "AABB.h"
#include "ICollider.h"

namespace
{
        void DebugTrace(const std::string& message)
        {
#if defined(_WIN32) && defined(_DEBUG)
                ::OutputDebugStringA(message.c_str());
#else
                (void)message;
#endif
        }

        constexpr const char* kLogPrefix = "[ContactLiteSystem] ";

        float GetAxisOverlap(const AABB& a, const AABB& b, int axis)
        {
                switch (axis)
                {
                case 0:
                        return std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x);
                case 1:
                        return std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y);
                case 2:
                        return std::min(a.max.z, b.max.z) - std::max(a.min.z, b.min.z);
                default:
                        return 0.0f;
                }
        }

        float GetAxisCenter(const AABB& a, int axis)
        {
                switch (axis)
                {
                case 0:
                        return (a.max.x + a.min.x) * 0.5f;
                case 1:
                        return (a.max.y + a.min.y) * 0.5f;
                case 2:
                        return (a.max.z + a.min.z) * 0.5f;
                default:
                        return 0.0f;
                }
        }

        float* SelectAxisComponent(float3& v, int axis)
        {
                switch (axis)
                {
                case 0:
                        return &v.x;
                case 1:
                        return &v.y;
                default:
                        return &v.z;
                }
        }
}

namespace Systems
{
        ContactLiteSystem::ContactLiteSystem()
                : Core::UpdateNode(Core::UpdatePhase::PostPhysics, 0)
                , m_bodies(nullptr)
                , m_penetrationTolerance(0.01f)
                , m_restitution(0.15f)
                , m_loggedMissingBodyList(false)
        {
        }

        void ContactLiteSystem::SetBodySource(const std::vector<SceneSpace::PhysicalEntity*>* bodies)
        {
                m_bodies = bodies;
                m_loggedMissingBodyList = false;
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << kLogPrefix << "Body source set. pointer=" << m_bodies;
                if (m_bodies)
                {
                        oss << " count=" << m_bodies->size();
                }
                oss << "\n";
                DebugTrace(oss.str());
#endif
        }

        void ContactLiteSystem::SetPenetrationTolerance(float tolerance)
        {
                m_penetrationTolerance = std::max(0.0f, tolerance);
        }

        void ContactLiteSystem::SetRestitution(float restitution)
        {
                m_restitution = std::clamp(restitution, 0.0f, 1.0f);
        }

        void ContactLiteSystem::OnUpdate(Core::UpdateContext& ctx)
        {
                (void)ctx;

                if (!m_bodies)
                {
                        if (!m_loggedMissingBodyList)
                        {
                                DebugTrace(std::string(kLogPrefix) + "No body list available. Contact resolution skipped.\n");
                                m_loggedMissingBodyList = true;
                        }
                        return;
                }

                const size_t bodyCount = m_bodies->size();
                if (bodyCount < 2)
                {
                        return;
                }

                size_t testedPairs = 0;
                size_t resolvedPairs = 0;

                for (size_t i = 0; i < bodyCount; ++i)
                {
                        SceneSpace::PhysicalEntity* bodyA = (*m_bodies)[i];
                        if (!bodyA)
                        {
                                continue;
                        }

                        Collision::ICollider* colliderA = bodyA->GetCollider();
                        if (!colliderA)
                        {
                                continue;
                        }

                        colliderA->UpdateBounds(bodyA->GetTransform());

                        for (size_t j = i + 1; j < bodyCount; ++j)
                        {
                                SceneSpace::PhysicalEntity* bodyB = (*m_bodies)[j];
                                if (!bodyB)
                                {
                                        continue;
                                }

                                Collision::ICollider* colliderB = bodyB->GetCollider();
                                if (!colliderB)
                                {
                                        continue;
                                }

                                colliderB->UpdateBounds(bodyB->GetTransform());

                                ++testedPairs;
                                if (!colliderA->CheckCollision(*colliderB))
                                {
                                        continue;
                                }

                                const AABB boundsA = colliderA->GetAABB();
                                const AABB boundsB = colliderB->GetAABB();

                                float overlaps[3];
                                overlaps[0] = GetAxisOverlap(boundsA, boundsB, 0);
                                overlaps[1] = GetAxisOverlap(boundsA, boundsB, 1);
                                overlaps[2] = GetAxisOverlap(boundsA, boundsB, 2);

                                int axis = 0;
                                float minOverlap = overlaps[0];
                                for (int k = 1; k < 3; ++k)
                                {
                                        if (overlaps[k] < minOverlap)
                                        {
                                                minOverlap = overlaps[k];
                                                axis = k;
                                        }
                                }

                                if (minOverlap <= 0.0f)
                                {
                                        continue;
                                }

                                const float pushDistance = minOverlap - m_penetrationTolerance;
                                if (pushDistance <= 0.0f)
                                {
                                        continue;
                                }

                                const float centerA = GetAxisCenter(boundsA, axis);
                                const float centerB = GetAxisCenter(boundsB, axis);
                                const float axisDir = (centerA < centerB) ? -1.0f : 1.0f;
                                const float halfPush = pushDistance * 0.5f;

                                Transform& transformA = bodyA->GetTransform();
                                Transform& transformB = bodyB->GetTransform();

                                float* axisComponentA = nullptr;
                                float* axisComponentB = nullptr;
                                switch (axis)
                                {
                                case 0:
                                        axisComponentA = &transformA.position.x;
                                        axisComponentB = &transformB.position.x;
                                        break;
                                case 1:
                                        axisComponentA = &transformA.position.y;
                                        axisComponentB = &transformB.position.y;
                                        break;
                                default:
                                        axisComponentA = &transformA.position.z;
                                        axisComponentB = &transformB.position.z;
                                        break;
                                }

                                *axisComponentA += axisDir * halfPush;
                                *axisComponentB -= axisDir * halfPush;

                                colliderA->UpdateBounds(transformA);
                                colliderB->UpdateBounds(transformB);

                                Physics::Physical& physA = bodyA->GetPhysical();
                                Physics::Physical& physB = bodyB->GetPhysical();

                                float* velA = SelectAxisComponent(physA.velocity, axis);
                                float* velB = SelectAxisComponent(physB.velocity, axis);
                                const float relativeVelocity = *velA - *velB;

                                if (std::fabs(relativeVelocity) > 1e-5f)
                                {
                                        const float impulse = -(1.0f + m_restitution) * relativeVelocity * 0.5f;
                                        *velA += impulse;
                                        *velB -= impulse;
                                }

                                ++resolvedPairs;
                        }
                }

#if defined(_DEBUG)
                if (resolvedPairs > 0)
                {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Resolved " << resolvedPairs << " overlaps out of "
                            << testedPairs << " tested pairs. tolerance=" << m_penetrationTolerance
                            << ", restitution=" << m_restitution << "\n";
                        DebugTrace(oss.str());
                }
#endif
        }
}
