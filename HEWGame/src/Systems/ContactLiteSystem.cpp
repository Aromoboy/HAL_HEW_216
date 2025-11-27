#include "Systems/ContactLiteSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
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
#include "Collision2D.h"
#include "SweetsPrototypeManifest.h"

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
        constexpr float kPlanarZTolerance = 1e-3f;

        struct ContactNormalClassification
        {
                float3 normal{ 0.0f, 0.0f, 0.0f };
                bool isGround{ false };
        };

        ContactNormalClassification ClassifyContactNormal(float3 normal, float maxSlopeCos)
        {
                ContactNormalClassification classification{};

                const float lenSq = normal.x * normal.x + normal.y * normal.y;
                if (lenSq <= 1e-6f)
                {
                        return classification;
                }

                const float invLen = 1.0f / std::sqrt(lenSq);
                normal.x *= invLen;
                normal.y *= invLen;

                const float slopeThreshold = std::clamp(maxSlopeCos, 0.0f, 1.0f);
                const float verticalComponent = std::clamp(std::fabs(normal.y), 0.0f, 1.0f);

                classification.isGround = verticalComponent >= slopeThreshold;
                if (classification.isGround)
                {
                        const float originalX = normal.x;
                        normal.x = 0.0f;
                        normal.y = (normal.y >= 0.0f) ? 1.0f : -1.0f;
#if defined(_DEBUG)
                        if (std::fabs(originalX) > 1e-4f)
                        {
                                std::ostringstream oss;
                                oss << kLogPrefix << "Snapped near-vertical normal to Y axis for stability."
                                        << " slopeThreshold=" << slopeThreshold << '\n';
                                DebugTrace(oss.str());
                        }
#endif
                }

                classification.normal = normal;
                return classification;
        }

        bool Is2DCollider(const Collision::ICollider* collider)
        {
                return dynamic_cast<const Collision::ICollider2D*>(collider) != nullptr;
        }

        bool Compute2DContact(const Collision::ICollider& a, const Collision::ICollider& b, Collision::ContactManifold2D& out)
        {
                const auto* colliderA2D = dynamic_cast<const Collision::ICollider2D*>(&a);
                const auto* colliderB2D = dynamic_cast<const Collision::ICollider2D*>(&b);
                if (!colliderA2D || !colliderB2D)
                {
                        return false;
                }

                if (colliderA2D->ComputeContact(*colliderB2D, out))
                {
                        return true;
                }

                return colliderB2D->ComputeContact(*colliderA2D, out);
        }

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

        bool IsPlayerBody(const Entities::PhysicalEntity* body)
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

        std::optional<SweetsSpace::SweetsType> ResolveSweetsTag(const std::string& tag)
        {
                return SweetsSpace::ParseSweetsType(tag);
        }

        bool IsSweetsTag(const std::string& tag)
        {
                return ResolveSweetsTag(tag).has_value();
        }

        std::string DescribeSweetsTag(const std::string& tag)
        {
                const auto resolved = ResolveSweetsTag(tag);
                if (resolved)
                {
                        return std::string(tag).append("/").append(SweetsSpace::ToString(*resolved));
                }
                return tag;
        }
}

namespace Systems
{
        ContactLiteSystem::ContactLiteSystem()
                : Core::UpdateNode(Core::UpdatePhase::PostPhysics, 0)
                , m_bodies(nullptr)
                , m_penetrationTolerance(0.01f)
                , m_restitution(0.15f)
                , m_staticFriction(0.8f)
                , m_dynamicFriction(0.65f)
                , m_maxSlopeCos(1.0f)
                , m_maxRestitutionVelocity(25.0f)
                , m_stableSupportThreshold(0.35f)
                , m_edgeSlideBoost(0.25f)
                , m_unstableVerticalDamping(0.35f)
                , m_loggedMissingBodyList(false)
                , m_loggedPlayerPresence(false)
                , m_loggedPlayerAbsence(false)
                , m_loggedPlayerPlatformContact(false)
                , m_loggedPlayerSweetsContact(false)
                , m_loggedSuppressedSweetsFloor(false)
                , m_loggedPlanarZSeparation(false)
                , m_loggedPlanarZIgnore(false)
        {
        }

        void ContactLiteSystem::SetBodySource(const std::vector<Entities::PhysicalEntity*>* bodies)
        {
                m_bodies = bodies;
                m_loggedMissingBodyList = false;
                m_loggedPlayerPresence = false;
                m_loggedPlayerAbsence = false;
                m_loggedPlayerPlatformContact = false;
                m_loggedPlayerSweetsContact = false;
                m_loggedSuppressedSweetsFloor = false;
                m_loggedPlanarZSeparation = false;
                m_loggedPlanarZIgnore = false;
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

        void ContactLiteSystem::SetFriction(float staticCoeff, float dynamicCoeff)
        {
                m_staticFriction = std::max(0.0f, staticCoeff);
                m_dynamicFriction = std::max(0.0f, dynamicCoeff);
        }

        void ContactLiteSystem::SetMaxSlopeCos(float maxSlopeCos)
        {
                m_maxSlopeCos = std::clamp(maxSlopeCos, 0.0f, 1.0f);
        }

        void ContactLiteSystem::SetMaxRestitutionVelocity(float maxRestitutionVelocity)
        {
                m_maxRestitutionVelocity = std::max(0.0f, maxRestitutionVelocity);
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
                const bool playerPresent = std::any_of(m_bodies->begin(), m_bodies->end(), [](Entities::PhysicalEntity* body)
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
                        Entities::PhysicalEntity* bodyA = (*m_bodies)[i];
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
                                Entities::PhysicalEntity* bodyB = (*m_bodies)[j];
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
                                        Entities::PhysicalEntity* other = IsPlayerBody(bodyA) ? bodyB : bodyA;
                                        if (other)
                                        {
                                                const std::string& otherTag = other->GetTag();
                                                const auto otherSweets = ResolveSweetsTag(otherTag);
                                                if (otherTag == "StageFoundation" && !m_loggedPlayerPlatformContact)
                                                {
                                                        std::ostringstream oss;
                                                        oss << kLogPrefix << "Player contact confirmed with stage platform via body tag='" << otherTag << "'.\n";
                                                        DebugTrace(oss.str());
                                                        m_loggedPlayerPlatformContact = true;
                                                }
                                                else if (otherSweets && !m_loggedPlayerSweetsContact)
                                                {
                                                        std::ostringstream oss;
                                                        oss << kLogPrefix << "Player contact confirmed with sweets tag='" << otherTag
                                                                << "' type='" << SweetsSpace::ToString(*otherSweets) << "'.\n";
                                                        DebugTrace(oss.str());
                                                        m_loggedPlayerSweetsContact = true;
                                                }
                                        }
                                }

                                const bool pairIs2D = Is2DCollider(colliderA) && Is2DCollider(colliderB);

                                const AABB boundsA = colliderA->GetAABB();
                                const AABB boundsB = colliderB->GetAABB();

                                float overlaps[3];
                                overlaps[0] = GetAxisOverlap(boundsA, boundsB, 0);
                                overlaps[1] = GetAxisOverlap(boundsA, boundsB, 1);
                                overlaps[2] = GetAxisOverlap(boundsA, boundsB, 2);

                                const float planarOverlapX = overlaps[0];
                                const float planarOverlapY = overlaps[1];
                                const float planarOverlapZ = overlaps[2];

                                if (!pairIs2D && planarOverlapZ < -kPlanarZTolerance)
                                {
#if defined(_DEBUG)
                                        if (!m_loggedPlanarZSeparation)
                                        {
                                                std::ostringstream oss;
                                                oss << kLogPrefix << "Skipping collision resolution due to Z separation exceeding planar tolerance. tags='"
                                                        << bodyA->GetTag() << "' vs '" << bodyB->GetTag() << "'"
                                                        << " overlapZ=" << planarOverlapZ
                                                        << " tolerance=" << kPlanarZTolerance << "\n";
                                                DebugTrace(oss.str());
                                                m_loggedPlanarZSeparation = true;
                                        }
#endif
                                        continue;
                                }

                                const bool ignoringZPenetration = pairIs2D || planarOverlapZ < kPlanarZTolerance;
#if defined(_DEBUG)
                                if (ignoringZPenetration && !m_loggedPlanarZIgnore)
                                {
                                        std::ostringstream oss;
                                        oss << kLogPrefix;
                                        if (pairIs2D)
                                        {
                                                oss << "Ignoring Z overlap for 2D collider pair '" << bodyA->GetTag()
                                                        << "' vs '" << bodyB->GetTag() << "'"
                                                        << " overlapZ=" << planarOverlapZ
                                                        << " (planar evaluation only).\n";
                                        }
                                        else
                                        {
                                                oss << "Proceeding with planar collision despite marginal Z overlap. tags='"
                                                        << bodyA->GetTag() << "' vs '" << bodyB->GetTag() << "'"
                                                        << " overlapZ=" << planarOverlapZ
                                                        << " tolerance=" << kPlanarZTolerance << "\n";
                                        }
                                        DebugTrace(oss.str());
                                        m_loggedPlanarZIgnore = true;
                                }
#endif

                                const float3 centerA{ GetAxisCenter(boundsA, 0), GetAxisCenter(boundsA, 1), GetAxisCenter(boundsA, 2) };
                                const float3 centerB{ GetAxisCenter(boundsB, 0), GetAxisCenter(boundsB, 1), GetAxisCenter(boundsB, 2) };
                                Physics::Physical& physA = bodyA->GetPhysical();
                                Physics::Physical& physB = bodyB->GetPhysical();
                                const bool bodyAStatic = !(physA.weight > 0.0f);
                                const bool bodyBStatic = !(physB.weight > 0.0f);

                                Collision::ContactManifold2D contact2D{};
                                bool usedSAT = pairIs2D && Compute2DContact(*colliderA, *colliderB, contact2D);
                                float3 contactNormal{ 0.0f, 0.0f, 0.0f };
                                float contactPenetration = 0.0f;
                                bool fallbackAABB = false;

                                if (usedSAT)
                                {
                                        contactNormal.x = contact2D.normal.x;
                                        contactNormal.y = contact2D.normal.y;
                                        const float lenSq = contactNormal.x * contactNormal.x + contactNormal.y * contactNormal.y;
                                        if (lenSq > 1e-6f)
                                        {
                                                const float invLen = 1.0f / std::sqrt(lenSq);
                                                contactNormal.x *= invLen;
                                                contactNormal.y *= invLen;
                                                contactPenetration = contact2D.penetration;
                                        }
                                        else
                                        {
                                                usedSAT = false;
                                        }
                                }

                                if (!usedSAT)
                                {
                                        int axis = 0;
                                        float minOverlap = planarOverlapX;
                                        const int axisLimit = pairIs2D ? 2 : 3;
                                        for (int ax = 1; ax < axisLimit; ++ax)
                                        {
                                                if (overlaps[ax] < minOverlap)
                                                {
                                                        minOverlap = overlaps[ax];
                                                        axis = ax;
                                                }
                                        }
                                        if (minOverlap <= 0.0f)
                                        {
                                                continue;
                                        }

                                        const float axisCenterA = GetAxisCenter(boundsA, axis);
                                        const float axisCenterB = GetAxisCenter(boundsB, axis);
                                        const float axisDir = (axisCenterA < axisCenterB) ? -1.0f : 1.0f;
                                        const float rawOverlap = minOverlap;
                                        if (rawOverlap <= m_penetrationTolerance)
                                        {
                                                continue;
                                        }

                                        contactPenetration = rawOverlap;

                                        switch (axis)
                                        {
                                        case 0:
                                                contactNormal = float3{ axisDir, 0.0f, 0.0f };
                                                break;
                                        case 1:
                                                contactNormal = float3{ 0.0f, axisDir, 0.0f };
                                                break;
                                        default:
                                                contactNormal = float3{ 0.0f, 0.0f, axisDir };
                                                break;
                                        }
                                        fallbackAABB = true;
                                }

                                const float3 separationVec{ centerA.x - centerB.x, centerA.y - centerB.y, 0.0f };
                                if (contactNormal.x == 0.0f && contactNormal.y == 0.0f)
                                {
                                        continue;
                                }
                                const float orientDot = contactNormal.x * separationVec.x + contactNormal.y * separationVec.y;
                                if (orientDot < 0.0f)
                                {
                                        contactNormal.x = -contactNormal.x;
                                        contactNormal.y = -contactNormal.y;
                                }

                                const ContactNormalClassification classification = ClassifyContactNormal(contactNormal, m_maxSlopeCos);
                                contactNormal = classification.normal;
                                if (contactNormal.x == 0.0f && contactNormal.y == 0.0f)
                                {
                                        continue;
                                }

                                float pushDistance = contactPenetration + m_penetrationTolerance;
                                if (pushDistance <= 0.0f)
                                {
                                        continue;
                                }

                                bool sweetsSupportPair = false;
                                bool unstableSupport = false;
                                bool topIsA = centerA.y > centerB.y;
                                float supportOverlapFraction = 1.0f;
                                float slideDir = 0.0f;
                                float verticalResolutionScale = 1.0f;

                                const bool treatedAsSupport = classification.isGround;
                                if (treatedAsSupport)
                                {
                                        const auto sweetsTypeA = ResolveSweetsTag(bodyA->GetTag());
                                        const auto sweetsTypeB = ResolveSweetsTag(bodyB->GetTag());
                                        const bool sweetsTagA = sweetsTypeA.has_value();
                                        const bool sweetsTagB = sweetsTypeB.has_value();
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
                                                const float stableDenominator = std::max(m_stableSupportThreshold, 1e-3f);
                                                verticalResolutionScale = unstableSupport
                                                        ? std::clamp(supportOverlapFraction / stableDenominator, 0.0f, 1.0f)
                                                        : 1.0f;

                                                const AABB& baseBounds = topIsA ? boundsB : boundsA;
                                                const AABB& topBounds = topIsA ? boundsA : boundsB;
                                                const float baseCenterX = GetAxisCenter(baseBounds, 0);
                                                const float topCenterX = GetAxisCenter(topBounds, 0);
                                                slideDir = (topCenterX >= baseCenterX) ? 1.0f : -1.0f;

#if defined(_DEBUG)
                                                {
                                                        const std::string baseTag = topIsA
                                                                ? DescribeSweetsTag(bodyB->GetTag())
                                                                : DescribeSweetsTag(bodyA->GetTag());
                                                        const std::string topTag = topIsA
                                                                ? DescribeSweetsTag(bodyA->GetTag())
                                                                : DescribeSweetsTag(bodyB->GetTag());
                                                        std::ostringstream oss;
                                                        oss << kLogPrefix << "Support analysis: top='" << topTag << "' base='"
                                                                << baseTag << "' overlapFraction=" << supportOverlapFraction
                                                                << " threshold=" << m_stableSupportThreshold
                                                                << " status=" << (unstableSupport ? "UNSTABLE" : "stable")
                                                                << " slideDir=" << slideDir
                                                                << " resolutionScale=" << verticalResolutionScale
                                                                << " usingSAT=" << (usedSAT ? "yes" : "no") << "\n";
                                                        DebugTrace(oss.str());
                                                }
#endif

                                                if (unstableSupport)
                                                {
                                                        Physics::Physical& topPhys = topIsA ? physA : physB;
                                                        if (topPhys.weight > 0.0f)
                                                        {
                                                                topPhys.velocity.x += slideDir * m_edgeSlideBoost;
#if defined(_DEBUG)
                                                                std::ostringstream oss;
                                                                oss << kLogPrefix << "Unstable support slide boost: top='"
                                                                        << (topIsA ? DescribeSweetsTag(bodyA->GetTag()) : DescribeSweetsTag(bodyB->GetTag()))
                                                                        << "' base='" << (topIsA ? DescribeSweetsTag(bodyB->GetTag()) : DescribeSweetsTag(bodyA->GetTag()))
                                                                        << "' overlapFraction=" << supportOverlapFraction
                                                                        << " boost=" << m_edgeSlideBoost
                                                                        << " dir=" << slideDir
                                                                        << " velocityX=" << topPhys.velocity.x << "\n";
                                                                DebugTrace(oss.str());
#endif
                                                        }
                                                }
                                        }
                                        pushDistance *= verticalResolutionScale;
                                }

                                float3 translationA{ 0.0f, 0.0f, 0.0f };
                                float3 translationB{ 0.0f, 0.0f, 0.0f };
                                if (!bodyAStatic && !bodyBStatic)
                                {
                                        const float halfPush = pushDistance * 0.5f;
                                        translationA.x += contactNormal.x * halfPush;
                                        translationA.y += contactNormal.y * halfPush;
                                        translationB.x -= contactNormal.x * halfPush;
                                        translationB.y -= contactNormal.y * halfPush;
                                }
                                else if (!bodyAStatic && bodyBStatic)
                                {
                                        translationA.x += contactNormal.x * pushDistance;
                                        translationA.y += contactNormal.y * pushDistance;
                                }
                                else if (bodyAStatic && !bodyBStatic)
                                {
                                        translationB.x -= contactNormal.x * pushDistance;
                                        translationB.y -= contactNormal.y * pushDistance;
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

                                Entities::TranslatePhysicalEntity(*bodyA, translationA);
                                Entities::TranslatePhysicalEntity(*bodyB, translationB);

                                const auto Project = [](const float3& v, const float3& dir)
                                {
                                        return v.x * dir.x + v.y * dir.y + v.z * dir.z;
                                };

                                const auto ApplyComponent = [&Project](Physics::Physical& phys, const float3& dir, float target)
                                {
                                        const float current = Project(phys.velocity, dir);
                                        const float delta = target - current;
                                        phys.velocity.x += dir.x * delta;
                                        phys.velocity.y += dir.y * delta;
                                        phys.velocity.z += dir.z * delta;
                                };

                                const float relativeVelocity = Project(float3{ physA.velocity.x - physB.velocity.x, physA.velocity.y - physB.velocity.y, 0.0f }, contactNormal);
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

                                        if (activeBodyCount > 0.0f)
                                        {
                                                const float relativeSpeed = std::fabs(relativeVelocity);
                                                const float restitutionScale = (m_maxRestitutionVelocity > 0.0f)
                                                        ? std::clamp(relativeSpeed / m_maxRestitutionVelocity, 0.0f, 1.0f)
                                                        : 1.0f;
                                                float impulse = -(1.0f + m_restitution * restitutionScale) * relativeVelocity / activeBodyCount;
#if defined(_DEBUG)
                                                if (restitutionScale < 1.0f)
                                                {
                                                        std::ostringstream oss;
                                                        oss << kLogPrefix << "Restitution scaled for low-speed pair '" << bodyA->GetTag()
                                                                << "' vs '" << bodyB->GetTag()
                                                                << "' relativeSpeed=" << relativeSpeed
                                                                << " threshold=" << m_maxRestitutionVelocity
                                                                << " scale=" << restitutionScale
                                                                << " impulse=" << impulse << "\n";
                                                        DebugTrace(oss.str());
                                                }
#endif
                                                if (treatedAsSupport && sweetsSupportPair && unstableSupport)
                                                {
                                                        const float previousImpulse = impulse;
                                                        impulse = 0.0f;
#if defined(_DEBUG)
                                                        std::ostringstream oss;
                                                        oss << kLogPrefix << "Restitution impulse bypassed for unstable sweets support pair '"
                                                                << bodyA->GetTag() << "' vs '" << bodyB->GetTag()
                                                                << "' previousImpulse=" << previousImpulse
                                                                << " overlapFraction=" << supportOverlapFraction
                                                                << " threshold=" << m_stableSupportThreshold << "\n";
                                                        DebugTrace(oss.str());
#endif
                                                }
                                                if (!bodyAStatic)
                                                {
                                                        physA.velocity.x += contactNormal.x * impulse;
                                                        physA.velocity.y += contactNormal.y * impulse;
                                                }
                                                if (!bodyBStatic)
                                                {
                                                        physB.velocity.x -= contactNormal.x * impulse;
                                                        physB.velocity.y -= contactNormal.y * impulse;
                                                }
                                        }
                                }

                                if (treatedAsSupport)
                                {
                                        if (!bodyAStatic && bodyBStatic)
                                        {
                                                const float previousVelocity = Project(physA.velocity, contactNormal);
                                                if (previousVelocity < 0.0f)
                                                {
                                                        if (unstableSupport && topIsA)
                                                        {
                                                                const float dampedVelocity = previousVelocity * m_unstableVerticalDamping;
                                                                const float adjustedVelocity = previousVelocity + (dampedVelocity - previousVelocity) * verticalResolutionScale;
                                                                ApplyComponent(physA, contactNormal, adjustedVelocity);
#if defined(_DEBUG)
                                                                std::ostringstream oss;
                                                                oss << kLogPrefix << "Unstable support damping applied to body '"
                                                                        << bodyA->GetTag() << "' (supported by '" << bodyB->GetTag()
                                                                        << "') velocityN " << previousVelocity
                                                                        << " -> " << adjustedVelocity
                                                                        << " (fraction=" << supportOverlapFraction
                                                                        << " scale=" << verticalResolutionScale << ")\n";
                                                                DebugTrace(oss.str());
#endif
                                                        }
                                                        else
                                                        {
                                                                ApplyComponent(physA, contactNormal, 0.0f);
#if defined(_DEBUG)
                                                                std::ostringstream oss;
                                                                oss << kLogPrefix << "Normal velocity clamped for body '" << bodyA->GetTag()
                                                                        << "' from " << previousVelocity << " due to static contact.\n";
                                                                DebugTrace(oss.str());
#endif
                                                        }
                                                }
                                        }
                                        if (bodyAStatic && !bodyBStatic)
                                        {
                                                const float previousVelocity = Project(physB.velocity, contactNormal);
                                                if (previousVelocity > 0.0f)
                                                {
                                                        if (unstableSupport && !topIsA)
                                                        {
                                                                const float dampedVelocity = previousVelocity * m_unstableVerticalDamping;
                                                                const float adjustedVelocity = previousVelocity + (dampedVelocity - previousVelocity) * verticalResolutionScale;
                                                                ApplyComponent(physB, contactNormal, adjustedVelocity);
#if defined(_DEBUG)
                                                                std::ostringstream oss;
                                                                oss << kLogPrefix << "Unstable support damping applied to body '"
                                                                        << bodyB->GetTag() << "' (supported by '" << bodyA->GetTag()
                                                                        << "') velocityN " << previousVelocity
                                                                        << " -> " << adjustedVelocity
                                                                        << " (fraction=" << supportOverlapFraction
                                                                        << " scale=" << verticalResolutionScale << ")\n";
                                                                DebugTrace(oss.str());
#endif
                                                        }
                                                        else
                                                        {
                                                                ApplyComponent(physB, contactNormal, 0.0f);
#if defined(_DEBUG)
                                                                std::ostringstream oss;
                                                                oss << kLogPrefix << "Normal velocity clamped for body '" << bodyB->GetTag()
                                                                        << "' from " << previousVelocity << " due to static contact.\n";
                                                                DebugTrace(oss.str());
#endif
                                                        }
                                                }
                                        }

                                        const float3 tangent{ -contactNormal.y, contactNormal.x, 0.0f };
                                        const float tangentLenSq = tangent.x * tangent.x + tangent.y * tangent.y;
                                        if (tangentLenSq > 1e-6f)
                                        {
                                                const float relativeTangentialVelocity = Project(float3{ physA.velocity.x - physB.velocity.x, physA.velocity.y - physB.velocity.y, 0.0f }, tangent);
                                                const bool allowStaticFriction = classification.isGround;
                                                const bool withinStaticThreshold = std::fabs(relativeTangentialVelocity) <= m_staticFriction;
                                                if (allowStaticFriction && withinStaticThreshold)
                                                {
                                                        const float sharedVelocity = (bodyAStatic && bodyBStatic)
                                                                ? 0.0f
                                                                : (Project(physA.velocity, tangent) + Project(physB.velocity, tangent)) * 0.5f;
                                                        if (!bodyAStatic)
                                                        {
                                                                ApplyComponent(physA, tangent, sharedVelocity);
                                                        }
                                                        if (!bodyBStatic)
                                                        {
                                                                ApplyComponent(physB, tangent, sharedVelocity);
                                                        }
#if defined(_DEBUG)
                                                        {
                                                                std::ostringstream oss;
                                                                oss << kLogPrefix << "Static friction applied to pair '"
                                                                        << bodyA->GetTag() << "' vs '" << bodyB->GetTag()
                                                                        << "' relativeTangentialVelocity=" << relativeTangentialVelocity
                                                                        << " threshold=" << m_staticFriction
                                                                        << " sharedVelocity=" << sharedVelocity << "\n";
                                                                DebugTrace(oss.str());
                                                        }
#endif
                                                }
                                                else
                                                {
                                                        if (!allowStaticFriction && withinStaticThreshold)
                                                        {
#if defined(_DEBUG)
                                                                std::ostringstream oss;
                                                                oss << kLogPrefix << "Static friction suppressed for steep contact between '"
                                                                        << bodyA->GetTag() << "' and '" << bodyB->GetTag()
                                                                        << "' |n.y|=" << std::fabs(contactNormal.y)
                                                                        << " threshold=" << m_maxSlopeCos
                                                                        << " relativeTangentialVelocity=" << relativeTangentialVelocity << "\n";
                                                                DebugTrace(oss.str());
#endif
                                                        }
                                                        const float frictionScale = std::clamp(1.0f - m_dynamicFriction * std::max(ctx.dt, 0.0f), 0.0f, 1.0f);
                                                        if (!bodyAStatic)
                                                        {
                                                                const float current = Project(physA.velocity, tangent);
                                                                ApplyComponent(physA, tangent, current * frictionScale);
                                                        }
                                                        if (!bodyBStatic)
                                                        {
                                                                const float current = Project(physB.velocity, tangent);
                                                                ApplyComponent(physB, tangent, current * frictionScale);
                                                        }
#if defined(_DEBUG)
                                                        {
                                                                std::ostringstream oss;
                                                                oss << kLogPrefix << "Dynamic friction applied to pair '"
                                                                        << bodyA->GetTag() << "' vs '" << bodyB->GetTag()
                                                                        << "' scale=" << frictionScale
                                                                        << " static=" << bodyAStatic << '/' << bodyBStatic
                                                                        << " tangentVelocityA=" << Project(physA.velocity, tangent)
                                                                        << " tangentVelocityB=" << Project(physB.velocity, tangent)
                                                                        << "\n";
                                                                DebugTrace(oss.str());
                                                        }
#endif
                                                }
                                        }
                                }

#if defined(_DEBUG)
                                {
                                        std::ostringstream oss;
                                        oss << kLogPrefix << "Resolved contact for '" << bodyA->GetTag() << "' vs '" << bodyB->GetTag()
                                                << "' normal=(" << contactNormal.x << "," << contactNormal.y << ",0)"
                                                << " penetration=" << contactPenetration
                                                << " pushDistance=" << pushDistance
                                                << " usedSAT=" << (usedSAT ? "yes" : "no")
                                                << " fallback=" << (fallbackAABB ? "yes" : "no");
                                        if (usedSAT)
                                        {
                                                oss << " contactPoint=(" << contact2D.contactPoint.x << "," << contact2D.contactPoint.y
                                                        << ")";
                                        }
                                        oss << " ground=" << (classification.isGround ? "yes" : "no");
                                        oss << "\n";
                                        DebugTrace(oss.str());
                                }
#endif
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
                            << ", restitution=" << m_restitution
                            << ", staticFriction=" << m_staticFriction
                            << ", dynamicFriction=" << m_dynamicFriction
                            << ", slopeCosLimit=" << m_maxSlopeCos
                            << ", maxRestitutionVelocity=" << m_maxRestitutionVelocity
                            << "\n";
                        DebugTrace(oss.str());
                }
#endif
        }
}
