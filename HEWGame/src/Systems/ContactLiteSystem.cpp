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

        struct FrameContactInfo
        {
                bool supported = false;
                bool stableSupport = false;
                bool unsupportedContact = false;
                bool translated = false;
                bool transformChanged = false;
                bool impulseApplied = false;
        };

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

                const float verticalSnapThreshold = std::clamp(verticalSnapCos, 0.0f, 1.0f);
                const float groundThreshold = std::clamp(groundMinCos, 0.0f, 1.0f);
                const float verticalComponent = std::clamp(std::fabs(normal.y), 0.0f, 1.0f);
                const float planarLenSq = (normal.x * normal.x) + (normal.z * normal.z);
                const float planarLen = std::sqrt(planarLenSq);

                classification.verticalComponent = verticalComponent;
                classification.isGround = verticalComponent >= groundThreshold;

                const bool allowSnap = classification.isGround
                        && verticalComponent >= verticalSnapThreshold
                        && planarLen <= 1e-3f;

                if (allowSnap)
                {
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

        struct LayerPairHash
        {
                size_t operator()(const std::pair<Collision::CollisionLayer, Collision::CollisionLayer>& value) const noexcept
                {
                        const size_t first = static_cast<size_t>(value.first);
                        const size_t second = static_cast<size_t>(value.second);
                        return (first << 16) ^ second;
                }
        };

        bool CanCollide(Collision::CollisionLayer a, Collision::CollisionLayer b)
        {
                const auto ordered = std::minmax(a, b);
                static const std::unordered_set<std::pair<Collision::CollisionLayer, Collision::CollisionLayer>, LayerPairHash>
                        kDisabledPairs{};

                return kDisabledPairs.find(ordered) == kDisabledPairs.end();
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
                , m_restingFrameThreshold(12)
                , m_restingSpeedThreshold(0.03f)
                , m_wakeSpeedThreshold(0.06f)
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
                std::unordered_set<Entities::PhysicalEntity*> observedBodies;
                observedBodies.reserve(sourceBodyCount);
                std::vector<Entities::PhysicalEntity*> activeBodies;
                activeBodies.reserve(sourceBodyCount);
                std::unordered_map<Entities::PhysicalEntity*, FrameContactInfo> frameContactInfo;
                frameContactInfo.reserve(sourceBodyCount);
                for (Entities::PhysicalEntity* body : *m_bodies)
                {
                        if (!body)
                        {
                                continue;
                        }

                        observedBodies.insert(body);

                        body->UpdateSleepState(body->GetPhysical().velocity);
                        if (body->IsSleeping())
                        {
                                continue;
                        }

                        const Physics::Physical& phys = body->GetPhysical();
                        const bool isPlayer = IsPlayerBody(body);

                        const bool kinematic = !(phys.weight > 0.0f);
                        if (kinematic && !m_includeKinematicBodies && !isPlayer)
                        {
                            continue;
                        }

                        FrameContactInfo info{};
                        info.transformChanged = body->IsTransformDirty();
                        frameContactInfo[body] = info;
                        activeBodies.push_back(body);
                }

                const size_t bodyCount = activeBodies.size();

                for (auto it = m_restingFrames.begin(); it != m_restingFrames.end();)
                {
                        if (observedBodies.find(it->first) == observedBodies.end())
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

                        body->SyncColliderBounds();
                        proxies.push_back(BodyProxy{ body, collider, body->GetCollisionLayer(), collider->GetAABB() });
                }

                float globalMinX = std::numeric_limits<float>::infinity();
                float globalMaxX = -std::numeric_limits<float>::infinity();
                float globalMinY = std::numeric_limits<float>::infinity();
                float globalMaxY = -std::numeric_limits<float>::infinity();

                for (const BodyProxy& proxy : proxies)
                {
                        globalMinX = std::min(globalMinX, proxy.bounds.min.x);
                        globalMaxX = std::max(globalMaxX, proxy.bounds.max.x);
                        globalMinY = std::min(globalMinY, proxy.bounds.min.y);
                        globalMaxY = std::max(globalMaxY, proxy.bounds.max.y);
                }

                int sweepAxis = 0;
                int secondaryPlanarAxis = 1;
                if (!proxies.empty())
                {
                        const float spanX = globalMaxX - globalMinX;
                        const float spanY = globalMaxY - globalMinY;
                        sweepAxis = (spanY > spanX) ? 1 : 0;
                        secondaryPlanarAxis = (sweepAxis == 0) ? 1 : 0;
                }

                std::sort(proxies.begin(), proxies.end(), [sweepAxis](const BodyProxy& a, const BodyProxy& b)
                {
                        return GetAxisMin(a.bounds, sweepAxis) < GetAxisMin(b.bounds, sweepAxis);
                });

                const size_t proxyCount = proxies.size();
                if (proxyCount < 2)
                {
                        // No contact pairs to evaluate this frame, but continue to resting detection.
                }

                const auto IsBodyResting = [&](Entities::PhysicalEntity* body)
                {
                        if (IsPlayerBody(body))
                        {
                                return false;
                        }

                        const auto it = m_restingFrames.find(body);
                        return it != m_restingFrames.end() && it->second.resting;
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
                                const AABB& boundsA = proxyA.bounds;
                                const AABB& boundsB = proxyB.bounds;

                                if (GetAxisMin(boundsB, sweepAxis) > GetAxisMax(boundsA, sweepAxis))
                                {
                                        break;
                                }

                                float overlaps[3];
                                overlaps[0] = GetAxisOverlap(boundsA, boundsB, 0);
                                overlaps[1] = GetAxisOverlap(boundsA, boundsB, 1);
                                overlaps[2] = GetAxisOverlap(boundsA, boundsB, 2);

                                if (overlaps[sweepAxis] <= 0.0f || overlaps[secondaryPlanarAxis] <= 0.0f)
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
                                const bool playerPlatformPair = (IsPlayerBody(bodyA) && layerB == Collision::CollisionLayer::Floor)
                                                || (IsPlayerBody(bodyB) && layerA == Collision::CollisionLayer::Floor);

                                const bool sweetsA = layerA == Collision::CollisionLayer::Sweets || IsSweetsTag(bodyA->GetTag());
                                const bool sweetsB = layerB == Collision::CollisionLayer::Sweets || IsSweetsTag(bodyB->GetTag());
                                const bool sweetsPair = sweetsA || sweetsB;

                                if (!pairIs2D && !playerPlatformPair && !sweetsPair && overlaps[2] < -kPlanarZTolerance)
                                {
                                        continue;
                                }

                                if (overlaps[0] <= 0.0f || overlaps[1] <= 0.0f)
                                {
                                        continue;
                                }

                                ++testedPairs;
                                if (!colliderA->CheckCollision(*colliderB))
                                {
                                        continue;
                                }

                                const float planarOverlapX = overlaps[0];
                                const float planarOverlapY = overlaps[1];
                                const float planarOverlapZ = overlaps[2];

                                const float3 centerA{ GetAxisCenter(boundsA, 0), GetAxisCenter(boundsA, 1), GetAxisCenter(boundsA, 2) };
                                const float3 centerB{ GetAxisCenter(boundsB, 0), GetAxisCenter(boundsB, 1), GetAxisCenter(boundsB, 2) };
                                Physics::Physical& physA = bodyA->GetPhysical();
                                Physics::Physical& physB = bodyB->GetPhysical();
                                const bool isPlayerA = IsPlayerBody(bodyA);
                                const bool isPlayerB = IsPlayerBody(bodyB);
                                const bool bodyAStatic = !(physA.weight > 0.0f) && !isPlayerA;
                                const bool bodyBStatic = !(physB.weight > 0.0f) && !isPlayerB;

                                Collision::ContactManifold2D contact2D{};
                                std::string contactMethod = "none";
                                bool usedSAT = false;
                                bool attemptedSAT = false;
                                float3 contactNormal{ 0.0f, 0.0f, 0.0f };
                                float contactPenetration = 0.0f;
                                bool fallbackAABB = false;

                                const auto* colliderA2D = dynamic_cast<const Collision::ICollider2D*>(colliderA);
                                const auto* colliderB2D = dynamic_cast<const Collision::ICollider2D*>(colliderB);
                                const auto* colliderAAsAABB = dynamic_cast<const Collision::AABBCollider*>(colliderA);
                                const auto* colliderBAsAABB = dynamic_cast<const Collision::AABBCollider*>(colliderB);
                                const bool colliderAIsAABB = colliderAAsAABB != nullptr;
                                const bool colliderBIsAABB = colliderBAsAABB != nullptr;
                                const bool contactPairHasAABB = colliderAIsAABB || colliderBIsAABB;

                                bool contactComputed = false;

                                const bool sweetsRequireSAT = sweetsPair && !playerPlatformPair;
                                const bool attemptSAT = pairIs2D || sweetsRequireSAT;

                                if (attemptSAT)
                                {
                                        attemptedSAT = true;
                                        ++satAttempts;

                                        if (pairIs2D)
                                        {
                                                usedSAT = Compute2DContact(*colliderA, *colliderB, contact2D);
                                        }
                                        else if (sweetsRequireSAT)
                                        {
                                                if (colliderA2D && colliderBIsAABB)
                                                {
                                                        usedSAT = Compute2DContactWithAABBProxy(*colliderA2D, *colliderBAsAABB, contact2D);
                                                }
                                                else if (colliderB2D && colliderAIsAABB)
                                                {
                                                        usedSAT = Compute2DContactWithAABBProxy(*colliderB2D, *colliderAAsAABB, contact2D);
                                                }
                                                else if (colliderA2D && colliderB2D)
                                                {
                                                        usedSAT = Compute2DContact(*colliderA, *colliderB, contact2D);
                                                }
                                        }

                                        if (usedSAT)
                                        {
                                                ++satSuccesses;
                                                contactMethod = "sat_2d";
                                                contactNormal.x = contact2D.normal.x;
                                                contactNormal.y = contact2D.normal.y;
                                                const float lenSq = contactNormal.x * contactNormal.x + contactNormal.y * contactNormal.y;
                                                if (lenSq > 1e-6f)
                                                {
                                                        const float invLen = 1.0f / std::sqrt(lenSq);
                                                        contactNormal.x *= invLen;
                                                        contactNormal.y *= invLen;
                                                        contactPenetration = contact2D.penetration;
                                                        contactComputed = true;
                                                }
                                                else
                                                {
                                                        usedSAT = false;
                                                }
                                        }
                                }

                                if (!contactComputed && contactPairHasAABB)
                                {
                                        const bool mixed2DPair = (colliderA2D || colliderB2D) && contactPairHasAABB;

                                        int axis = (planarOverlapX < planarOverlapY) ? 0 : 1;
                                        const float axisCenterA = GetAxisCenter(boundsA, axis);
                                        const float axisCenterB = GetAxisCenter(boundsB, axis);
                                        const float axisDir = (axisCenterA < axisCenterB) ? -1.0f : 1.0f;
                                        contactPenetration = (axis == 0) ? planarOverlapX : planarOverlapY;

                                        if (mixed2DPair)
                                        {
                                                contactNormal = (axis == 0) ? float3{ axisDir, 0.0f, 0.0f } : float3{ 0.0f, axisDir, 0.0f };
                                                contactMethod = "aabb_planar_mixed";
                                                contactComputed = true;
                                        }
                                        else if (colliderAIsAABB && colliderBIsAABB)
                                        {
                                                contactNormal = (axis == 0) ? float3{ axisDir, 0.0f, 0.0f } : float3{ 0.0f, axisDir, 0.0f };
                                                contactMethod = "aabb_planar";
                                                contactComputed = true;
                                        }
                                }

                                fallbackAABB = false;

                                if (!usedSAT && !contactComputed)
                                {
                                        continue;
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

                                const bool bodyAResting = IsBodyResting(bodyA);
                                const bool bodyBResting = IsBodyResting(bodyB);

                                const float penetrationError = std::max(contactPenetration - m_penetrationTolerance, 0.0f);
                                if (penetrationError <= 0.0f)
                                {
                                        continue;
                                }

                                const float pushDistance = std::clamp(
                                        m_baumgarteFactor * penetrationError,
                                        0.0f,
                                        m_maxPenetrationCorrection);
                                if (pushDistance <= 0.0f)
                                {
                                        continue;
                                }

                                float3 resolutionNormal = contactNormal;
                                resolutionNormal.z = 0.0f;
                                const float resolutionLenSq = resolutionNormal.x * resolutionNormal.x + resolutionNormal.y * resolutionNormal.y;
                                if (resolutionLenSq <= 1e-6f)
                                {
                                        continue;
                                }
                                const float invResolutionLen = 1.0f / std::sqrt(resolutionLenSq);
                                resolutionNormal.x *= invResolutionLen;
                                resolutionNormal.y *= invResolutionLen;
                                contactNormal = resolutionNormal;

                                const bool treatedAsSupport = isGroundContact;
                                if (isGroundContact)
                                {
                                        const bool stableSupport = classification.verticalComponent >= m_stableSupportThreshold;
                                        frameContactInfo[bodyA].supported = true;
                                        frameContactInfo[bodyB].supported = true;
                                        frameContactInfo[bodyA].stableSupport = frameContactInfo[bodyA].stableSupport || stableSupport;
                                        frameContactInfo[bodyB].stableSupport = frameContactInfo[bodyB].stableSupport || stableSupport;
                                }
                                else
                                {
                                        frameContactInfo[bodyA].unsupportedContact = true;
                                        frameContactInfo[bodyB].unsupportedContact = true;
                                        bodyA->Wake();
                                        bodyB->Wake();
                                }

                                float3 translationA{ 0.0f, 0.0f, 0.0f };
                                float3 translationB{ 0.0f, 0.0f, 0.0f };
                                if (!bodyAStatic && !bodyBStatic)
                                {
                                        const float halfPush = pushDistance * 0.5f;
                                        translationA.x += resolutionNormal.x * halfPush;
                                        translationA.y += resolutionNormal.y * halfPush;
                                        translationB.x -= resolutionNormal.x * halfPush;
                                        translationB.y -= resolutionNormal.y * halfPush;
                                }
                                else if (!bodyAStatic && bodyBStatic)
                                {
                                        translationA.x += resolutionNormal.x * pushDistance;
                                        translationA.y += resolutionNormal.y * pushDistance;
                                }
                                else if (bodyAStatic && !bodyBStatic)
                                {
                                        translationB.x -= resolutionNormal.x * pushDistance;
                                        translationB.y -= resolutionNormal.y * pushDistance;
                                }
                                else
                                {
                                        continue;
                                }

                                translationA.z = 0.0f;
                                translationB.z = 0.0f;

                                if (translationA.x != 0.0f || translationA.y != 0.0f || translationA.z != 0.0f)
                                {
                                        frameContactInfo[bodyA].translated = true;
                                }
                                if (translationB.x != 0.0f || translationB.y != 0.0f || translationB.z != 0.0f)
                                {
                                        frameContactInfo[bodyB].translated = true;
                                }

                                Entities::TranslatePhysicalEntity(*bodyA, translationA);
                                Entities::TranslatePhysicalEntity(*bodyB, translationB);


                                const bool allowImpulseA = !bodyAStatic;
                                const bool allowImpulseB = !bodyBStatic;
                                const bool modifyVelocityA = allowImpulseA && !IsPlayerBody(bodyA);
                                const bool modifyVelocityB = allowImpulseB && !IsPlayerBody(bodyB);
                                const float massA = bodyAStatic ? std::numeric_limits<float>::infinity() : physA.weight;
                                const float massB = bodyBStatic ? std::numeric_limits<float>::infinity() : physB.weight;
                                const auto SplitImpulse = [&](float impulse)
                                {
                                        float impulseA = 0.0f;
                                        float impulseB = 0.0f;
                                        if (std::isinf(massA))
                                        {
                                                impulseB = -impulse;
                                        }
                                        else if (std::isinf(massB))
                                        {
                                                impulseA = impulse;
                                        }
                                        else
                                        {
                                                const float massSum = massA + massB;
                                                if (massSum > 0.0f)
                                                {
                                                        impulseA = impulse * (massB / massSum);
                                                        impulseB = -impulse * (massA / massSum);
                                                }
                                        }
                                        return std::pair<float, float>{ impulseA, impulseB };
                                };

                                const float2 normal2{ contactNormal.x, contactNormal.y };
                                const float2 relativeVelocity{ physA.velocity.x - physB.velocity.x, physA.velocity.y - physB.velocity.y };
                                const float relativeNormalSpeed = relativeVelocity.x * normal2.x + relativeVelocity.y * normal2.y;

                                const float invMassA = std::isinf(massA) ? 0.0f : (1.0f / massA);
                                const float invMassB = std::isinf(massB) ? 0.0f : (1.0f / massB);
                                const float invMassSum = invMassA + invMassB;

                                float vn = relativeNormalSpeed;
                                float appliedNormalImpulseA = 0.0f;
                                float appliedNormalImpulseB = 0.0f;
                                float appliedTangentImpulseA = 0.0f;
                                float appliedTangentImpulseB = 0.0f;
                                if (vn < 0.0f && invMassSum > 0.0f)
                                {
                                        vn = std::clamp(vn, -m_maxRestitutionVelocity, 0.0f);
                                        float jn = -(1.0f + m_restitution) * vn / invMassSum;

                                        const auto [jnA, jnB] = SplitImpulse(jn);
                                        const float2 normalImpulseA{ normal2.x * jnA, normal2.y * jnA };
                                        const float2 normalImpulseB{ normal2.x * jnB, normal2.y * jnB };

                                        if (modifyVelocityA)
                                        {
                                                physA.velocity.x += normalImpulseA.x;
                                                physA.velocity.y += normalImpulseA.y;
                                                appliedNormalImpulseA = jnA;
                                        }
                                        if (modifyVelocityB)
                                        {
                                                physB.velocity.x += normalImpulseB.x;
                                                physB.velocity.y += normalImpulseB.y;
                                                appliedNormalImpulseB = jnB;
                                        }

                                        float2 tangentialVelocity
                                        {
                                                relativeVelocity.x - relativeNormalSpeed * normal2.x,
                                                relativeVelocity.y - relativeNormalSpeed * normal2.y
                                        };

                                        const float tangentialSpeedSq = tangentialVelocity.x * tangentialVelocity.x + tangentialVelocity.y * tangentialVelocity.y;
                                        if (tangentialSpeedSq > 1e-6f)
                                        {
                                                const float tangentialSpeed = std::sqrt(tangentialSpeedSq);
                                                const float2 tangentDir{ tangentialVelocity.x / tangentialSpeed, tangentialVelocity.y / tangentialSpeed };

                                                if (invMassSum > 0.0f)
                                                {
                                                        const float relTangentSpeed = relativeVelocity.x * tangentDir.x + relativeVelocity.y * tangentDir.y;
                                                        float jt = -relTangentSpeed / invMassSum;
                                                        const float maxStatic = m_staticFriction * std::fabs(jn);
                                                        if (std::fabs(jt) > maxStatic)
                                                        {
                                                                const float sign = (relTangentSpeed >= 0.0f) ? 1.0f : -1.0f;
                                                                jt = -sign * m_dynamicFriction * std::fabs(jn);
                                                        }

                                                        const auto [jtA, jtB] = SplitImpulse(jt);
                                                        const float2 tangentImpulseA{ tangentDir.x * jtA, tangentDir.y * jtA };
                                                        const float2 tangentImpulseB{ tangentDir.x * jtB, tangentDir.y * jtB };

                                                        if (modifyVelocityA)
                                                        {
                                                                physA.velocity.x += tangentImpulseA.x;
                                                                physA.velocity.y += tangentImpulseA.y;
                                                                appliedTangentImpulseA = jtA;
                                                        }
                                                        if (modifyVelocityB)
                                                        {
                                                                physB.velocity.x += tangentImpulseB.x;
                                                                physB.velocity.y += tangentImpulseB.y;
                                                                appliedTangentImpulseB = jtB;
                                                        }
                                                }
                                        }
                                }

                                if (std::fabs(appliedNormalImpulseA) > 0.0f || std::fabs(appliedTangentImpulseA) > 0.0f)
                                {
                                        frameContactInfo[bodyA].impulseApplied = true;
                                        bodyA->Wake();
                                }
                                if (std::fabs(appliedNormalImpulseB) > 0.0f || std::fabs(appliedTangentImpulseB) > 0.0f)
                                {
                                        frameContactInfo[bodyB].impulseApplied = true;
                                        bodyB->Wake();
                                }

                                ++resolvedPairs;
                        }
                }

                for (auto& [body, info] : frameContactInfo)
                {
                        if (!body)
                        {
                                continue;
                        }

                        if (info.transformChanged || info.translated || info.unsupportedContact || info.impulseApplied)
                        {
                                body->Wake();
                                auto restingIt = m_restingFrames.find(body);
                                if (restingIt != m_restingFrames.end())
                                {
                                        restingIt->second.frames = 0;
                                        restingIt->second.resting = false;
                                }
                        }
                }

                const auto IsResting = [&](const Entities::PhysicalEntity& body, bool supportedThisFrame)
                {
                        if (IsPlayerBody(&body))
                        {
                                return false;
                        }

                        if (!supportedThisFrame)
                        {
                                return false;
                        }

                        const Physics::Physical& phys = body.GetPhysical();
                        if (!(phys.weight > 0.0f))
                        {
                                return false;
                        }

                        const float speedSq = phys.velocity.x * phys.velocity.x
                                + phys.velocity.y * phys.velocity.y
                                + phys.velocity.z * phys.velocity.z;
                        const float restingSpeedSq = m_restingSpeedThreshold * m_restingSpeedThreshold;
                        if (speedSq >= restingSpeedSq)
                        {
                                return false;
                        }

                        const float externalSpeedSq = phys.force.x * phys.force.x
                                + phys.force.y * phys.force.y
                                + phys.force.z * phys.force.z;
                        if (externalSpeedSq > 0.0f)
                        {
                                return false;
                        }

                        const auto infoIt = frameContactInfo.find(const_cast<Entities::PhysicalEntity*>(&body));
                        const bool translated = (infoIt != frameContactInfo.end()) && infoIt->second.translated;
                        const bool transformChanged = (infoIt != frameContactInfo.end()) && infoIt->second.transformChanged;
                        const bool unsupportedContact = (infoIt != frameContactInfo.end()) && infoIt->second.unsupportedContact;
                        const bool impulseApplied = (infoIt != frameContactInfo.end()) && infoIt->second.impulseApplied;
                        const bool stableSupport = (infoIt != frameContactInfo.end()) && infoIt->second.stableSupport;

                        if (!stableSupport)
                        {
                                return false;
                        }

                        if (translated || transformChanged || unsupportedContact || impulseApplied)
                        {
                                return false;
                        }

                        if (body.IsTransformDirty())
                        {
                                return false;
                        }

                        return true;
                };

                for (Entities::PhysicalEntity* body : activeBodies)
                {
                        const auto infoIt = frameContactInfo.find(body);
                        const bool supported = (infoIt != frameContactInfo.end()) && infoIt->second.supported;

                        RestingState& resting = m_restingFrames[body];
                        if (IsResting(*body, supported))
                        {
                                resting.frames += 1;
                                resting.resting = resting.frames >= m_restingFrameThreshold;
                        }
                        else
                        {
                                resting.frames = 0;
                                resting.resting = false;
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
