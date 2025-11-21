#include "Systems/ContactLiteSystem.h"

#include <algorithm>
#include <array>
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
#include "CollisionLayer.h"

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

        bool IsPlayerBody(const SceneSpace::PhysicalEntity* body)
        {
                return body && body->GetTag() == "player";
        }

        const char* ToString(Collision::CollisionLayer layer)
        {
                switch (layer)
                {
                case Collision::CollisionLayer::Player:
                        return "Player";
                case Collision::CollisionLayer::Floor:
                        return "Floor";
                case Collision::CollisionLayer::Sweets:
                        return "Sweets";
                default:
                        return "Default";
                }
        }

        bool CanCollide(Collision::CollisionLayer a, Collision::CollisionLayer b)
        {
                const bool sweetsFloorPair =
                        (a == Collision::CollisionLayer::Sweets && b == Collision::CollisionLayer::Floor)
                        || (a == Collision::CollisionLayer::Floor && b == Collision::CollisionLayer::Sweets);
                if (sweetsFloorPair)
                {
                        return false;
                }

                return true;
        }

        bool IsSweetsTag(const std::string& tag)
        {
                static const std::array<const char*, 3> kSweetsTags{ { "Cake", "Pudding", "Sherbet" } };
                for (const char* sweetsTag : kSweetsTags)
                {
                        if (tag == sweetsTag)
                        {
                                return true;
                        }
                }
                return false;
        }
}

namespace Systems
{
        ContactLiteSystem::ContactLiteSystem()
                : Core::UpdateNode(Core::UpdatePhase::PostPhysics, 0)
                , m_bodies(nullptr)
                , m_penetrationTolerance(0.01f)
                , m_restitution(0.15f)
                , m_stableSupportThreshold(0.35f)
                , m_edgeSlideBoost(0.25f)
                , m_unstableVerticalDamping(0.35f)
                , m_loggedMissingBodyList(false)
                , m_loggedPlayerPresence(false)
                , m_loggedPlayerAbsence(false)
                , m_loggedPlayerPlatformContact(false)
                , m_loggedPlayerSweetsContact(false)
                , m_loggedSuppressedSweetsFloor(false)
        {
        }

        void ContactLiteSystem::SetBodySource(const std::vector<SceneSpace::PhysicalEntity*>* bodies)
        {
                m_bodies = bodies;
                m_loggedMissingBodyList = false;
                m_loggedPlayerPresence = false;
                m_loggedPlayerAbsence = false;
                m_loggedPlayerPlatformContact = false;
                m_loggedPlayerSweetsContact = false;
                m_loggedSuppressedSweetsFloor = false;
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

        void ContactLiteSystem::SetStableSupportThreshold(float threshold)
        {
                m_stableSupportThreshold = std::clamp(threshold, 0.0f, 1.0f);
        }

        void ContactLiteSystem::SetEdgeSlideBoost(float boost)
        {
                m_edgeSlideBoost = std::max(0.0f, boost);
        }

        void ContactLiteSystem::SetUnstableVerticalDamping(float damping)
        {
                m_unstableVerticalDamping = std::clamp(damping, 0.0f, 1.0f);
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
                const bool playerPresent = std::any_of(m_bodies->begin(), m_bodies->end(), [](SceneSpace::PhysicalEntity* body)
                {
                        return IsPlayerBody(body);
                });
                if (playerPresent)
                {
                        if (!m_loggedPlayerPresence)
                        {
                                DebugTrace(std::string(kLogPrefix) + "Player body detected in contact solver input. Collisions with avatar will be evaluated.\n");
                                m_loggedPlayerPresence = true;
                        }
                        m_loggedPlayerAbsence = false;
                }
                else
                {
                        if (!m_loggedPlayerAbsence)
                        {
                                DebugTrace(std::string(kLogPrefix) + "Player body missing from contact solver input; avatar collisions disabled.\n");
                                m_loggedPlayerAbsence = true;
                        }
                        m_loggedPlayerPresence = false;
                }

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

                        const Collision::CollisionLayer layerA = bodyA->GetCollisionLayer();

                        bodyA->SyncColliderBounds();

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

                                const Collision::CollisionLayer layerB = bodyB->GetCollisionLayer();

                                bodyB->SyncColliderBounds();

                                if (!CanCollide(layerA, layerB))
                                {
#if defined(_DEBUG)
                                        if (!m_loggedSuppressedSweetsFloor &&
                                                ((layerA == Collision::CollisionLayer::Sweets && layerB == Collision::CollisionLayer::Floor)
                                                        || (layerA == Collision::CollisionLayer::Floor && layerB == Collision::CollisionLayer::Sweets)))
                                        {
                                                std::ostringstream oss;
                                                oss << kLogPrefix << "Collision suppressed for layers "
                                                        << ToString(layerA) << " vs " << ToString(layerB)
                                                        << " (bodies '" << bodyA->GetTag() << "' and '" << bodyB->GetTag() << "').\n";
                                                DebugTrace(oss.str());
                                                m_loggedSuppressedSweetsFloor = true;
                                        }
#endif
                                        continue;
                                }

                                ++testedPairs;
                                if (!colliderA->CheckCollision(*colliderB))
                                {
                                        continue;
                                }

                                const bool playerPair = IsPlayerBody(bodyA) || IsPlayerBody(bodyB);
                                if (playerPair)
                                {
                                        SceneSpace::PhysicalEntity* other = IsPlayerBody(bodyA) ? bodyB : bodyA;
                                        if (other)
                                        {
                                                const std::string& otherTag = other->GetTag();
                                                if (otherTag == "StageFoundation" && !m_loggedPlayerPlatformContact)
                                                {
                                                        std::ostringstream oss;
                                                        oss << kLogPrefix << "Player contact confirmed with stage platform via body tag='" << otherTag << "'.\n";
                                                        DebugTrace(oss.str());
                                                        m_loggedPlayerPlatformContact = true;
                                                }
                                                else if (IsSweetsTag(otherTag) && !m_loggedPlayerSweetsContact)
                                                {
                                                        std::ostringstream oss;
                                                        oss << kLogPrefix << "Player contact confirmed with sweets tag='" << otherTag << "'.\n";
                                                        DebugTrace(oss.str());
                                                        m_loggedPlayerSweetsContact = true;
                                                }
                                        }
                                }

                                const AABB boundsA = colliderA->GetAABB();
                                const AABB boundsB = colliderB->GetAABB();

                                float overlaps[3];
                                overlaps[0] = GetAxisOverlap(boundsA, boundsB, 0);
                                overlaps[1] = GetAxisOverlap(boundsA, boundsB, 1);
                                overlaps[2] = GetAxisOverlap(boundsA, boundsB, 2);

                                const float planarOverlapX = overlaps[0];
                                const float planarOverlapY = overlaps[1];
                                const bool suppressedZAxis = overlaps[2] < std::min(planarOverlapX, planarOverlapY);
                                const bool chooseX = planarOverlapX <= planarOverlapY;
                                const int axis = chooseX ? 0 : 1;
                                const float minOverlap = chooseX ? planarOverlapX : planarOverlapY;

#if defined(_DEBUG)
                                if (suppressedZAxis && minOverlap > 0.0f)
                                {
                                        std::ostringstream oss;
                                        oss << kLogPrefix << "Suppressed Z-axis overlap (" << overlaps[2]
                                            << ") to maintain planar constraint. Using axis " << axis
                                            << " overlap=" << minOverlap << "\n";
                                        DebugTrace(oss.str());
                                }
#endif

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
                                Physics::Physical& physA = bodyA->GetPhysical();
                                Physics::Physical& physB = bodyB->GetPhysical();
                                const bool bodyAStatic = !(physA.weight > 0.0f);
                                const bool bodyBStatic = !(physB.weight > 0.0f);

                                float3 translationA{ 0.0f, 0.0f, 0.0f };
                                float3 translationB{ 0.0f, 0.0f, 0.0f };
                                float* translationComponentA = SelectAxisComponent(translationA, axis);
                                float* translationComponentB = SelectAxisComponent(translationB, axis);

                                if (!bodyAStatic && !bodyBStatic)
                                {
                                        const float halfPush = pushDistance * 0.5f;
                                        const float axisPush = axisDir * halfPush;
                                        *translationComponentA += axisPush;
                                        *translationComponentB -= axisPush;
                                }
                                else if (!bodyAStatic && bodyBStatic)
                                {
                                        const float axisPush = axisDir * pushDistance;
                                        *translationComponentA += axisPush;
                                }
                                else if (bodyAStatic && !bodyBStatic)
                                {
                                        const float axisPush = -axisDir * pushDistance;
                                        *translationComponentB += axisPush;
                                }
                                else
                                {
                                        continue;
                                }

                                if (translationA.z != 0.0f)
                                {
#if defined(_DEBUG)
                                        std::ostringstream oss;
                                        oss << kLogPrefix << "Body '" << bodyA->GetTag()
                                            << "' requested Z translation " << translationA.z
                                            << "; overriding to maintain planar constraint.\n";
                                        DebugTrace(oss.str());
#endif
                                        translationA.z = 0.0f;
                                }
                                if (translationB.z != 0.0f)
                                {
#if defined(_DEBUG)
                                        std::ostringstream oss;
                                        oss << kLogPrefix << "Body '" << bodyB->GetTag()
                                            << "' requested Z translation " << translationB.z
                                            << "; overriding to maintain planar constraint.\n";
                                        DebugTrace(oss.str());
#endif
                                        translationB.z = 0.0f;
                                }

                                SceneSpace::TranslatePhysicalEntity(*bodyA, translationA);
                                SceneSpace::TranslatePhysicalEntity(*bodyB, translationB);

                                float* velA = SelectAxisComponent(physA.velocity, axis);
                                float* velB = SelectAxisComponent(physB.velocity, axis);
                                const float relativeVelocity = *velA - *velB;

                                bool sweetsSupportPair = false;
                                bool unstableSupport = false;
                                bool topIsA = centerA > centerB;
                                float supportOverlapFraction = 1.0f;
                                float slideDir = 0.0f;

                                if (axis == 1)
                                {
                                        const bool sweetsTagA = IsSweetsTag(bodyA->GetTag());
                                        const bool sweetsTagB = IsSweetsTag(bodyB->GetTag());
                                        const bool foundationA = bodyA->GetTag() == "StageFoundation";
                                        const bool foundationB = bodyB->GetTag() == "StageFoundation";
                                        sweetsSupportPair = (sweetsTagA && sweetsTagB)
                                                || (sweetsTagA && foundationB)
                                                || (foundationA && sweetsTagB);

                                        if (sweetsSupportPair)
                                        {
                                                const float widthA = boundsA.max.x - boundsA.min.x;
                                                const float widthB = boundsB.max.x - boundsB.min.x;
                                                const float minWidth = std::max(std::min(widthA, widthB), 1e-3f);
                                                supportOverlapFraction = std::clamp(planarOverlapX / minWidth, 0.0f, 1.0f);
                                                unstableSupport = supportOverlapFraction < m_stableSupportThreshold;

                                                const AABB& baseBounds = topIsA ? boundsB : boundsA;
                                                const AABB& topBounds = topIsA ? boundsA : boundsB;
                                                const float baseCenterX = GetAxisCenter(baseBounds, 0);
                                                const float topCenterX = GetAxisCenter(topBounds, 0);
                                                slideDir = (topCenterX >= baseCenterX) ? 1.0f : -1.0f;

                                                if (unstableSupport)
                                                {
                                                        Physics::Physical& topPhys = topIsA ? physA : physB;
                                                        if (topPhys.weight > 0.0f)
                                                        {
                                                                topPhys.velocity.x += slideDir * m_edgeSlideBoost;
#if defined(_DEBUG)
                                                                std::ostringstream oss;
                                                                oss << kLogPrefix << "Unstable support: overlapFraction="
                                                                        << supportOverlapFraction << " (< threshold "
                                                                        << m_stableSupportThreshold << ") between '"
                                                                        << bodyA->GetTag() << "' and '" << bodyB->GetTag()
                                                                        << "'. Slide boost=" << m_edgeSlideBoost
                                                                        << " dir=" << slideDir << "\n";
                                                                DebugTrace(oss.str());
#endif
                                                        }
                                                }
                                        }
                                }

                                if (std::fabs(relativeVelocity) > 1e-5f)
                                {
                                        float activeBodyCount = 0.0f;
                                        if (!bodyAStatic)
                                        {
                                                activeBodyCount += 1.0f;
                                        }
                                        if (!bodyBStatic)
                                        {
                                                activeBodyCount += 1.0f;
                                        }

                                        if (activeBodyCount <= 0.0f)
                                        {
                                                continue;
                                        }

                                        const float impulse = -(1.0f + m_restitution) * relativeVelocity / activeBodyCount;
                                        if (!bodyAStatic)
                                        {
                                                *velA += impulse;
                                        }
                                        if (!bodyBStatic)
                                        {
                                                *velB -= impulse;
                                        }
                                }

                                if (axis == 1)
                                {
                                        if (!bodyAStatic && bodyBStatic && *velA < 0.0f)
                                        {
#if defined(_DEBUG)
                                                const float previousVelocity = *velA;
#endif
                                                if (unstableSupport && topIsA)
                                                {
                                                        *velA *= m_unstableVerticalDamping;
#if defined(_DEBUG)
                                                        std::ostringstream oss;
                                                        oss << kLogPrefix << "Unstable support damping applied to body '"
                                                                << bodyA->GetTag() << "' velocityY " << previousVelocity
                                                                << " -> " << *velA << " (fraction=" << supportOverlapFraction
                                                                << ")\n";
                                                        DebugTrace(oss.str());
#endif
                                                }
                                                else
                                                {
                                                        *velA = 0.0f;
#if defined(_DEBUG)
                                                std::ostringstream oss;
                                                oss << kLogPrefix << "Vertical velocity clamped for body '" << bodyA->GetTag()
                                                    << "' from " << previousVelocity << " due to static contact.\n";
                                                DebugTrace(oss.str());
#endif
                                                }
                                        }
                                        if (!bodyBStatic && bodyAStatic && *velB < 0.0f)
                                        {
#if defined(_DEBUG)
                                                const float previousVelocity = *velB;
#endif
                                                if (unstableSupport && !topIsA)
                                                {
                                                        *velB *= m_unstableVerticalDamping;
#if defined(_DEBUG)
                                                        std::ostringstream oss;
                                                        oss << kLogPrefix << "Unstable support damping applied to body '"
                                                                << bodyB->GetTag() << "' velocityY " << previousVelocity
                                                                << " -> " << *velB << " (fraction=" << supportOverlapFraction
                                                                << ")\n";
                                                        DebugTrace(oss.str());
#endif
                                                }
                                                else
                                                {
                                                        *velB = 0.0f;
#if defined(_DEBUG)
                                                std::ostringstream oss;
                                                oss << kLogPrefix << "Vertical velocity clamped for body '" << bodyB->GetTag()
                                                    << "' from " << previousVelocity << " due to static contact.\n";
                                                DebugTrace(oss.str());
#endif
                                                }
                                        }
                                }

                                if (physA.velocity.z != 0.0f)
                                {
#if defined(_DEBUG)
                                        std::ostringstream oss;
                                        oss << kLogPrefix << "Body '" << bodyA->GetTag()
                                            << "' velocity.z=" << physA.velocity.z
                                            << " reset to zero post-contact.\n";
                                        DebugTrace(oss.str());
#endif
                                        physA.velocity.z = 0.0f;
                                }
                                if (physB.velocity.z != 0.0f)
                                {
#if defined(_DEBUG)
                                        std::ostringstream oss;
                                        oss << kLogPrefix << "Body '" << bodyB->GetTag()
                                            << "' velocity.z=" << physB.velocity.z
                                            << " reset to zero post-contact.\n";
                                        DebugTrace(oss.str());
#endif
                                        physB.velocity.z = 0.0f;
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
