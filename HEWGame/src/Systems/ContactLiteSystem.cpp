#include "Systems/ContactLiteSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <sstream>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "AABB.h"
#include "AABBCollider.h"
#include "ICollider.h"
#include "CollisionLayer.h"
#include "Collision2D.h"
#include "SweetsPrototypeManifest.h"

static_assert(std::is_class_v<Systems::ContactLiteSystem>, "ContactLiteSystem declaration must remain visible to the implementation.");

namespace
{
        void DebugTrace(const std::string& message)
        {
#ifdef _WIN32
                OutputDebugStringA(message.c_str());
#else
                (void)message;
#endif
        }

        constexpr const char* kLogPrefix = "[ContactLiteSystem] ";
        constexpr float kPlanarZTolerance = 1e-3f;
        constexpr float kFoundationSatBypassScale = 2.0f;
        constexpr float kSatPlanarOverlapThreshold = 0.05f;
        constexpr float kMinResolutionPush = 1e-4f;

        struct ContactNormalClassification
        {
                float3 normal{ 0.0f, 0.0f, 0.0f };
                float verticalComponent{ 0.0f };
                bool isGround{ false };
                bool isVerticalSnapped{ false };
        };

        ContactNormalClassification ClassifyContactNormal(float3 normal, float verticalSnapCos, float groundMinCos)
        {
                ContactNormalClassification classification{};

                const float lenSq = normal.x * normal.x + normal.y * normal.y + normal.z * normal.z;
                if (lenSq <= 1e-6f)
                {
                        return classification;
                }

                const float invLen = 1.0f / std::sqrt(lenSq);
                normal.x *= invLen;
                normal.y *= invLen;
                normal.z *= invLen;

                const float snapThreshold = std::clamp(verticalSnapCos, 0.0f, 1.0f);
                const float groundThreshold = std::clamp(groundMinCos, 0.0f, 1.0f);
                const float verticalComponent = std::clamp(std::fabs(normal.y), 0.0f, 1.0f);
                constexpr float kMinSnapVerticalComponent = 0.999f;

                classification.verticalComponent = verticalComponent;
                classification.isGround = verticalComponent >= groundThreshold;
                if (verticalComponent >= snapThreshold && verticalComponent >= kMinSnapVerticalComponent)
                {
                        const float originalX = normal.x;
                        const float originalZ = normal.z;
                        normal.x = 0.0f;
                        normal.y = (normal.y >= 0.0f) ? 1.0f : -1.0f;
                        normal.z = 0.0f;
                        classification.isVerticalSnapped = true;
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

        std::vector<float2> BuildAABBHull(const AABB& bounds)
        {
                return {
                        float2{ bounds.min.x, bounds.min.y },
                        float2{ bounds.max.x, bounds.min.y },
                        float2{ bounds.max.x, bounds.max.y },
                        float2{ bounds.min.x, bounds.max.y },
                };
        }

        bool Compute2DContactWithAABBProxy(const Collision::ICollider2D& collider2D,
                const Collision::AABBCollider& aabbCollider,
                Collision::ContactManifold2D& out)
        {
                const AABB bounds = aabbCollider.GetAABB();
                const float thickness = std::max(bounds.max.z - bounds.min.z, 0.0f);
                Collision::ConvexHullCollider2D proxy(BuildAABBHull(bounds), float2{ 0.0f, 0.0f }, thickness, aabbCollider.GetLayer());
                return collider2D.ComputeContact(proxy, out);
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

        float GetAxisMin(const AABB& a, int axis)
        {
                switch (axis)
                {
                case 0:
                        return a.min.x;
                case 1:
                        return a.min.y;
                case 2:
                        return a.min.z;
                default:
                        return 0.0f;
                }
        }

        float GetAxisMax(const AABB& a, int axis)
        {
                switch (axis)
                {
                case 0:
                        return a.max.x;
                case 1:
                        return a.max.y;
                case 2:
                        return a.max.z;
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
                , m_verticalSnapCos(0.98f)
                , m_groundMinCos(0.5f)
                , m_maxRestitutionVelocity(25.0f)
                , m_stableSupportThreshold(0.35f)
                , m_edgeSlideBoost(0.25f)
                , m_unstableVerticalDamping(0.35f)
                , m_baumgarteFactor(0.2f)
                , m_maxPenetrationCorrection(0.1f)
                , m_minLinearSpeed(0.05f)
                , m_minVerticalSpeed(0.02f)
                , m_minTangentialSpeed(0.01f)
                , m_restingFrames{}
                , m_restingFrameThreshold(8)
                , m_restingSpeedThreshold(0.025f)
                , m_includeKinematicBodies(true)
                , m_allowKinematicPlayerCollision(true)
        {
        }

        void ContactLiteSystem::SetBodySource(const std::vector<Entities::PhysicalEntity*>* bodies)
        {
                m_bodies = bodies;
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

        void ContactLiteSystem::SetVerticalSnapCos(float verticalSnapCos)
        {
                m_verticalSnapCos = std::clamp(verticalSnapCos, 0.0f, 1.0f);
        }

        void ContactLiteSystem::SetGroundMinCos(float groundMinCos)
        {
                m_groundMinCos = std::clamp(groundMinCos, 0.0f, 1.0f);
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

        void ContactLiteSystem::SetBaumgarteFactor(float factor)
        {
                m_baumgarteFactor = std::clamp(factor, 0.0f, 1.0f);
        }

        void ContactLiteSystem::SetMaxPenetrationCorrection(float maxCorrection)
        {
                m_maxPenetrationCorrection = std::max(0.0f, maxCorrection);
        }

        void ContactLiteSystem::SetMinLinearSpeed(float minSpeed)
        {
                m_minLinearSpeed = std::max(0.0f, minSpeed);
        }

        void ContactLiteSystem::SetMinVerticalSpeed(float minSpeed)
        {
                m_minVerticalSpeed = std::max(0.0f, minSpeed);
        }

        void ContactLiteSystem::SetMinTangentialSpeed(float minSpeed)
        {
                m_minTangentialSpeed = std::max(0.0f, minSpeed);
        }

        void ContactLiteSystem::SetIncludeKinematicBodies(bool include)
        {
                m_includeKinematicBodies = include;
        }

        void ContactLiteSystem::SetAllowKinematicPlayerCollision(bool allow)
        {
                m_allowKinematicPlayerCollision = allow;
        }

        void ContactLiteSystem::OnUpdate(Core::UpdateContext& ctx)
        {
                if (!m_bodies)
                {
                        return;
                }

                const size_t sourceBodyCount = m_bodies->size();
                std::vector<Entities::PhysicalEntity*> activeBodies;
                activeBodies.reserve(sourceBodyCount);
                for (Entities::PhysicalEntity* body : *m_bodies)
                {
                        if (!body)
                        {
                                continue;
                        }

                        body->UpdateSleepState(body->GetPhysical().velocity);
                        if (body->IsSleeping())
                        {
                                continue;
                        }

                        const bool isPlayer = IsPlayerBody(body);
                        const bool kinematic = !(body->GetPhysical().weight > 0.0f);
                        if (kinematic && !m_includeKinematicBodies && !(isPlayer && m_allowKinematicPlayerCollision))
                        {
                                continue;
                        }

                        activeBodies.push_back(body);
                }

                const size_t bodyCount = activeBodies.size();
                if (bodyCount < 2)
                {
                        return;
                }

                std::unordered_set<Entities::PhysicalEntity*> activeSet;
                activeSet.reserve(bodyCount);
                for (Entities::PhysicalEntity* body : activeBodies)
                {
                        if (!body)
                        {
                                continue;
                        }

                        activeSet.insert(body);
                        const Physics::Physical& phys = body->GetPhysical();
                        const float planarSpeedSq = phys.velocity.x * phys.velocity.x + phys.velocity.y * phys.velocity.y;
                        const float speed = std::sqrt(planarSpeedSq);
                        if (speed < m_restingSpeedThreshold)
                        {
                                m_restingFrames[body] += 1;
                        }
                        else
                        {
                                m_restingFrames[body] = 0;
                        }
                }

                for (auto it = m_restingFrames.begin(); it != m_restingFrames.end();)
                {
                        if (activeSet.find(it->first) == activeSet.end())
                        {
                                it = m_restingFrames.erase(it);
                        }
                        else
                        {
                                ++it;
                        }
                }

                struct BodyProxy
                {
                        Entities::PhysicalEntity* body;
                        Collision::ICollider* collider;
                        Collision::CollisionLayer layer;
                        AABB bounds;
                };

                std::vector<BodyProxy> proxies;
                proxies.reserve(bodyCount);

                for (Entities::PhysicalEntity* body : activeBodies)
                {
                        if (!body)
                        {
                                continue;
                        }

                        Collision::ICollider* collider = body->GetCollider();
                        if (!collider)
                        {
                                continue;
                        }

                        if (body->IsTransformDirty())
                        {
                                body->SyncColliderBounds();
                        }
                        proxies.push_back(BodyProxy{ body, collider, body->GetCollisionLayer(), collider->GetAABB() });
                }

                const int sweepAxis = 0;
                const int planarTestAxis = 1;

                std::sort(proxies.begin(), proxies.end(), [sweepAxis](const BodyProxy& a, const BodyProxy& b)
                {
                        return GetAxisMin(a.bounds, sweepAxis) < GetAxisMin(b.bounds, sweepAxis);
                });

                const size_t proxyCount = proxies.size();
                if (proxyCount < 2)
                {
                        return;
                }

                const auto IsResting = [&](Entities::PhysicalEntity* body)
                {
                        const auto it = m_restingFrames.find(body);
                        return it != m_restingFrames.end() && it->second >= m_restingFrameThreshold;
                };

                size_t testedPairs = 0;
                size_t resolvedPairs = 0;
                size_t satAttempts = 0;
                size_t satSuccesses = 0;

                for (size_t i = 0; i < proxyCount; ++i)
                {
                        const BodyProxy& proxyA = proxies[i];
                        Entities::PhysicalEntity* bodyA = proxyA.body;
                        Collision::ICollider* colliderA = proxyA.collider;
                        const Collision::CollisionLayer layerA = proxyA.layer;

                        for (size_t j = i + 1; j < proxyCount; ++j)
                        {
                                const BodyProxy& proxyB = proxies[j];
                                const float sweepOverlapX = GetAxisOverlap(proxyA.bounds, proxyB.bounds, sweepAxis);
                                if (sweepOverlapX <= 0.0f)
                                {
                                        break;
                                }

                                const float sweepOverlapY = GetAxisOverlap(proxyA.bounds, proxyB.bounds, planarTestAxis);
                                if (sweepOverlapY <= 0.0f)
                                {
                                        continue;
                                }

                                Entities::PhysicalEntity* bodyB = proxyB.body;
                                Collision::ICollider* colliderB = proxyB.collider;
                                const Collision::CollisionLayer layerB = proxyB.layer;

                                if (!CanCollide(layerA, layerB))
                                {
                                        continue;
                                }

                                const bool pairIs2D = Is2DCollider(colliderA) && Is2DCollider(colliderB);
                                if (!pairIs2D)
                                {
                                        const float planarOverlapZ = GetAxisOverlap(proxyA.bounds, proxyB.bounds, 2);
                                        if (planarOverlapZ < -kPlanarZTolerance)
                                        {
                                                continue;
                                        }
                                }

                                ++testedPairs;
                                if (!colliderA->CheckCollision(*colliderB))
                                {
                                        continue;
                                }

                                const AABB boundsA = proxyA.bounds;
                                const AABB boundsB = proxyB.bounds;

                                float overlaps[3];
                                overlaps[0] = sweepOverlapX;
                                overlaps[1] = sweepOverlapY;
                                overlaps[2] = GetAxisOverlap(boundsA, boundsB, 2);

                                const float planarOverlapX = overlaps[0];
                                const float planarOverlapY = overlaps[1];
                                const float planarOverlapZ = overlaps[2];

                                if (planarOverlapX <= 0.0f || planarOverlapY <= 0.0f)
                                {
                                        continue;
                                }

                                if (!pairIs2D && planarOverlapZ < -kPlanarZTolerance)
                                {
                                        continue;
                                }

                                const float3 centerA{ GetAxisCenter(boundsA, 0), GetAxisCenter(boundsA, 1), GetAxisCenter(boundsA, 2) };
                                const float3 centerB{ GetAxisCenter(boundsB, 0), GetAxisCenter(boundsB, 1), GetAxisCenter(boundsB, 2) };
                                Physics::Physical& physA = bodyA->GetPhysical();
                                Physics::Physical& physB = bodyB->GetPhysical();
                                const bool bodyAStatic = !(physA.weight > 0.0f);
                                const bool bodyBStatic = !(physB.weight > 0.0f);

                                Collision::ContactManifold2D contact2D{};
                                std::string contactMethod = "none";
                                const bool sweetsPair = IsSweetsTag(bodyA->GetTag()) || IsSweetsTag(bodyB->GetTag());
                                bool usedSAT = false;
                                bool attemptedSAT = false;
                                const float minPlanarOverlap = std::min(planarOverlapX, planarOverlapY);
                                const bool planarPairForSAT = pairIs2D || (planarOverlapZ <= kPlanarZTolerance);
                                const bool allowSAT = pairIs2D && planarPairForSAT && (minPlanarOverlap > kSatPlanarOverlapThreshold);
                                float3 contactNormal{ 0.0f, 0.0f, 0.0f };
                                float contactPenetration = 0.0f;
                                bool fallbackAABB = false;

                                const bool foundationPair = colliderA->GetLayer() == Collision::CollisionLayer::Floor
                                        || colliderB->GetLayer() == Collision::CollisionLayer::Floor;
                                const bool staticFoundationPair = foundationPair && (bodyAStatic || bodyBStatic);
                                const float verticalOverlap = planarOverlapY;
                                const bool shallowFoundationContact = staticFoundationPair
                                        && verticalOverlap > m_penetrationTolerance
                                        && verticalOverlap <= (m_penetrationTolerance * kFoundationSatBypassScale);

                                if (shallowFoundationContact)
                                {
                                        const float axisDir = (centerA.y < centerB.y) ? -1.0f : 1.0f;
                                        contactNormal = float3{ 0.0f, axisDir, 0.0f };
                                        contactPenetration = verticalOverlap;
                                        fallbackAABB = true;
                                        contactMethod = "foundation_aabb";
                                        usedSAT = false;
                                }
                                else
                                {
                                        attemptedSAT = allowSAT;
                                        if (attemptedSAT)
                                        {
                                                ++satAttempts;
                                                usedSAT = Compute2DContact(*colliderA, *colliderB, contact2D);
                                        }
                                        if (usedSAT)
                                        {
                                                ++satSuccesses;
                                        }
                                        contactMethod = usedSAT ? "sat_2d" : contactMethod;

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

                                        const bool allowFallback = !sweetsPair || foundationPair || !attemptedSAT || !usedSAT;
                                        if (!usedSAT && allowFallback)
                                        {
                                                int axis = -1;
                                                float minOverlap = std::numeric_limits<float>::max();
                                                const int axisLimit = 2;
                                                for (int ax = 0; ax < axisLimit; ++ax)
                                                {
                                                        if (overlaps[ax] > m_penetrationTolerance && overlaps[ax] < minOverlap)
                                                        {
                                                                minOverlap = overlaps[ax];
                                                                axis = ax;
                                                        }
                                                }
                                                if (axis < 0)
                                                {
                                                        continue;
                                                }

                                                const float axisCenterA = GetAxisCenter(boundsA, axis);
                                                const float axisCenterB = GetAxisCenter(boundsB, axis);
                                                const float axisDir = (axisCenterA < axisCenterB) ? -1.0f : 1.0f;

                                                contactPenetration = minOverlap;

                                                switch (axis)
                                                {
                                                case 0:
                                                        contactNormal = float3{ axisDir, 0.0f, 0.0f };
                                                        break;
                                                default:
                                                        contactNormal = float3{ 0.0f, axisDir, 0.0f };
                                                        break;
                                                }
                                                fallbackAABB = true;
                                                contactMethod = "aabb_overlap";
                                        }
                                        else if (!usedSAT)
                                        {
                                                continue;
                                        }
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

                                const ContactNormalClassification classification = ClassifyContactNormal(contactNormal, m_verticalSnapCos, m_groundMinCos);
                                contactNormal = classification.normal;
                                if (contactNormal.x == 0.0f && contactNormal.y == 0.0f)
                                {
                                        continue;
                                }
                                const float absContactNormalY = std::fabs(contactNormal.y);
                                const bool slopeAllowsSupport = absContactNormalY >= m_maxSlopeCos;
                                bool isGroundContact = classification.isGround && slopeAllowsSupport;

                                const bool bodyAResting = IsResting(bodyA);
                                const bool bodyBResting = IsResting(bodyB);
                                const float restPenetrationThreshold = m_penetrationTolerance * 2.0f;

                                const float penetrationError = std::max(contactPenetration - m_penetrationTolerance, 0.0f);
                                const bool belowBaumgarteThreshold = contactPenetration < restPenetrationThreshold;
                                float baumgarteFactor = m_baumgarteFactor;
                                if (!bodyAStatic && !bodyBStatic)
                                {
                                        const float relativeSpeedSq = (physA.velocity.x - physB.velocity.x) * (physA.velocity.x - physB.velocity.x)
                                                + (physA.velocity.y - physB.velocity.y) * (physA.velocity.y - physB.velocity.y);
                                        const float relativeSpeed = std::sqrt(relativeSpeedSq);
                                        if (relativeSpeed < m_minVerticalSpeed)
                                        {
                                                baumgarteFactor *= 0.5f;
                                        }
                                }

                                float pushDistance = belowBaumgarteThreshold
                                        ? 0.0f
                                        : std::min(baumgarteFactor * penetrationError, m_maxPenetrationCorrection);
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

                                const bool treatedAsSupport = isGroundContact;
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

                                                if (unstableSupport)
                                                {
                                                        Physics::Physical& topPhys = topIsA ? physA : physB;
                                                        if (topPhys.weight > 0.0f)
                                                        {
                                                                const bool topResting = topIsA ? bodyAResting : bodyBResting;
                                                                const float planarSpeedSq = topPhys.velocity.x * topPhys.velocity.x + topPhys.velocity.y * topPhys.velocity.y;
                                                                const float slideThresholdSq = (m_restingSpeedThreshold * 2.0f) * (m_restingSpeedThreshold * 2.0f);
                                                                if (!topResting && planarSpeedSq >= slideThresholdSq)
                                                                {
                                                                        const float boostScale = std::clamp(supportOverlapFraction, 0.0f, 1.0f);
                                                                        topPhys.velocity.x += slideDir * (m_edgeSlideBoost * boostScale);
                                                                }
                                                        }
                                                }
                                        }
                                        pushDistance *= verticalResolutionScale;
                                }

                                if (pushDistance < kMinResolutionPush)
                                {
                                        continue;
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
                                        translationA.z = 0.0f;
                                }
                                if (translationB.z != 0.0f)
                                {
                                        translationB.z = 0.0f;
                                }

                                Entities::TranslatePhysicalEntity(*bodyA, translationA);
                                Entities::TranslatePhysicalEntity(*bodyB, translationB);

                                const bool allowImpulseA = !bodyAStatic && (!bodyAResting || contactPenetration > restPenetrationThreshold);
                                const bool allowImpulseB = !bodyBStatic && (!bodyBResting || contactPenetration > restPenetrationThreshold);

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

                                const auto ClampVelocity = [&](Physics::Physical& phys, const float3& normal, const float3& tangent,
                                        bool hasTangent, bool groundContact, bool supportContact, const std::string& tag, bool allowModification)
                                {
                                        if (phys.weight <= 0.0f || !allowModification)
                                        {
                                                return;
                                        }

                                        const auto clampComponent = [](float& component, float minSpeed)
                                        {
                                                if (std::fabs(component) < minSpeed)
                                                {
                                                        component = 0.0f;
                                                        return true;
                                                }
                                                return false;
                                        };

                                        bool clampedLinear = false;
                                        const float planarSpeedSq = phys.velocity.x * phys.velocity.x + phys.velocity.y * phys.velocity.y;
                                        const float planarSpeed = std::sqrt(planarSpeedSq);
                                        if (planarSpeed < (m_minLinearSpeed * 0.5f))
                                        {
                                                phys.velocity.x = 0.0f;
                                                phys.velocity.y = 0.0f;
                                                clampedLinear = true;
                                        }
                                        clampedLinear |= clampComponent(phys.velocity.x, m_minLinearSpeed);
                                        clampedLinear |= clampComponent(phys.velocity.y, m_minLinearSpeed);
                                        clampedLinear |= clampComponent(phys.velocity.z, m_minLinearSpeed);

                                        bool clampedNormal = false;
                                        bool clampedTangent = false;
                                        if (groundContact)
                                        {
                                                const float normalComponent = Project(phys.velocity, normal);
                                                if (std::fabs(normalComponent) < m_minVerticalSpeed)
                                                {
                                                        ApplyComponent(phys, normal, 0.0f);
                                                        clampedNormal = true;
                                                }

                                                if (hasTangent)
                                                {
                                                        const float tangentialComponent = Project(phys.velocity, tangent);
                                                        if (std::fabs(tangentialComponent) < m_minTangentialSpeed)
                                                        {
                                                                ApplyComponent(phys, tangent, 0.0f);
                                                                clampedTangent = true;
                                                        }
                                                }
                                        }

                                };

                                const float3 tangent{ -contactNormal.y, contactNormal.x, 0.0f };
                                const float tangentLenSq = tangent.x * tangent.x + tangent.y * tangent.y;
                                const bool hasTangent = tangentLenSq > 1e-6f;

                                const float relativeVelocity = Project(float3{ physA.velocity.x - physB.velocity.x, physA.velocity.y - physB.velocity.y, 0.0f }, contactNormal);
                                if (std::fabs(relativeVelocity) > m_minVerticalSpeed)
                                {
                                        float activeBodyCount = 0.0f;
                                        if (allowImpulseA)
                                        {
                                                activeBodyCount += 1.0f;
                                        }
                                        if (allowImpulseB)
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
                                                if (treatedAsSupport && sweetsSupportPair && unstableSupport)
                                                {
                                                        impulse = 0.0f;
                                                }
                                                if (allowImpulseA)
                                                {
                                                        physA.velocity.x += contactNormal.x * impulse;
                                                        physA.velocity.y += contactNormal.y * impulse;
                                                }
                                                if (allowImpulseB)
                                                {
                                                        physB.velocity.x -= contactNormal.x * impulse;
                                                        physB.velocity.y -= contactNormal.y * impulse;
                                                }
                                        }
                                }

                                if (treatedAsSupport)
                                {
                                        if (!bodyAStatic && bodyBStatic && allowImpulseA)
                                        {
                                                const float previousVelocity = Project(physA.velocity, contactNormal);
                                                if (previousVelocity < 0.0f)
                                                {
                                                        if (unstableSupport && topIsA)
                                                        {
                                                                const float dampedVelocity = previousVelocity * m_unstableVerticalDamping;
                                                                const float adjustedVelocity = previousVelocity + (dampedVelocity - previousVelocity) * verticalResolutionScale;
                                                                ApplyComponent(physA, contactNormal, adjustedVelocity);
                                                        }
                                                        else
                                                        {
                                                                ApplyComponent(physA, contactNormal, 0.0f);
                                                        }
                                                }
                                        }
                                        if (bodyAStatic && !bodyBStatic && allowImpulseB)
                                        {
                                                const float previousVelocity = Project(physB.velocity, contactNormal);
                                                if (previousVelocity > 0.0f)
                                                {
                                                        if (unstableSupport && !topIsA)
                                                        {
                                                                const float dampedVelocity = previousVelocity * m_unstableVerticalDamping;
                                                                const float adjustedVelocity = previousVelocity + (dampedVelocity - previousVelocity) * verticalResolutionScale;
                                                                ApplyComponent(physB, contactNormal, adjustedVelocity);
                                                        }
                                                        else
                                                        {
                                                                ApplyComponent(physB, contactNormal, 0.0f);
                                                        }
                                                }
                                        }

                                        if (hasTangent)
                                        {
                                        const float relativeTangentialVelocity = Project(float3{ physA.velocity.x - physB.velocity.x, physA.velocity.y - physB.velocity.y, 0.0f }, tangent);
                                        const float absRelativeTangentialVelocity = std::fabs(relativeTangentialVelocity);
                                        if (absRelativeTangentialVelocity < m_minTangentialSpeed)
                                        {
                                                // Deadzone to prevent jitter
                                        }
                                        else
                                        {
                                        const bool slopeAllowsStaticFriction = absContactNormalY >= m_maxSlopeCos;
                                        const bool allowStaticFriction = isGroundContact && slopeAllowsStaticFriction && (m_staticFriction > 0.0f);
                                        const bool withinStaticThreshold = absRelativeTangentialVelocity < m_minTangentialSpeed;
                                        if (allowStaticFriction && withinStaticThreshold)
                                        {
                                                        const float sharedVelocity = (bodyAStatic && bodyBStatic)
                                                                ? 0.0f
                                                                : (Project(physA.velocity, tangent) + Project(physB.velocity, tangent)) * 0.5f;
                                                        if (allowImpulseA)
                                                        {
                                                                ApplyComponent(physA, tangent, sharedVelocity);
                                                        }
                                                        if (allowImpulseB)
                                                        {
                                                                ApplyComponent(physB, tangent, sharedVelocity);
                                                        }
                                                }
                                                else
                                                {
                                                        if (!allowStaticFriction && withinStaticThreshold)
                                                        {
                                                        }
                                                        else if (allowStaticFriction && !withinStaticThreshold)
                                                        {
                                                        }
                                                        const float frictionScale = std::clamp(1.0f - m_dynamicFriction * std::max(ctx.dt, 0.0f), 0.0f, 1.0f);
                                                        if (allowImpulseA)
                                                        {
                                                                const float current = Project(physA.velocity, tangent);
                                                                ApplyComponent(physA, tangent, current * frictionScale);
                                                        }
                                                        if (allowImpulseB)
                                                        {
                                                                const float current = Project(physB.velocity, tangent);
                                                                ApplyComponent(physB, tangent, current * frictionScale);
                                                        }
                                                }
                                        }
                                        }

                                }

                                ClampVelocity(physA, contactNormal, tangent, hasTangent, isGroundContact, treatedAsSupport, bodyA->GetTag(), allowImpulseA);
                                ClampVelocity(physB, contactNormal, tangent, hasTangent, isGroundContact, treatedAsSupport, bodyB->GetTag(), allowImpulseB);

                                if (physA.velocity.z != 0.0f)
                                {
                                        physA.velocity.z = 0.0f;
                                }
                                if (physB.velocity.z != 0.0f)
                                {
                                        physB.velocity.z = 0.0f;
                                }

                                ++resolvedPairs;
                        }
                }

                if (testedPairs > 0 || resolvedPairs > 0)
                {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Broadphase tested=" << testedPairs
                                << " accepted=" << resolvedPairs
                                << " satAttempted=" << satAttempts
                                << " satSucceeded=" << satSuccesses
                                << "\n";
                        DebugTrace(oss.str());
                }

        }
}
