#include "Meteors/MeteorsContacts.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <sstream>
#include <vector>

#include "CollisionLayer.h"
#include "Meteors/BaseMeteors.h"
#include "Meteors/MeteorsKeyUtils.h"
#include "MeteorsPrototypeManifest.h"
#include "Systems/Physics/PhysicsMachine.h"

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
    void DebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    void DebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

    constexpr const char* kLogPrefix = "[MeteorsManager] ";
    constexpr float kGlueCooldownSeconds = 2.0f;

    std::string NormalizeTag(const std::string& tag)
    {
        return MeteorsSpace::NormalizeKey(tag);
    }

    bool IsCreamTag(const std::string& tag)
    {
        static const std::string kCreamKey = NormalizeTag(::MeteorsSpace::ToString(::MeteorsSpace::MeteorsType::Mercury));
        return NormalizeTag(tag) == kCreamKey;
    }

    bool IsBlockedTag(const std::string& tag)
    {
        static const std::unordered_set<std::string> kBlockedTags = {
            NormalizeTag("fairy"),
            NormalizeTag("player"),
        };
        return kBlockedTags.contains(NormalizeTag(tag));
    }

    struct ImpactImpulseInfo
    {
        b2BodyId fallingBody{ b2_nullBodyId };
        b2BodyId gluedBody{ b2_nullBodyId };
        float fallingMass{ 0.0f };
        float relativeSpeedY{ 0.0f };
        float impulse{ 0.0f };
        b2Vec2 fallingVelocity{};
        b2Vec2 gluedVelocity{};
    };

    std::optional<ImpactImpulseInfo> TryGetImpactImpulseInfo(
        const MeteorsSpace::ActiveMeteors& fallingFruit,
        const MeteorsSpace::ActiveMeteors& gluedFruit)
    {
        const b2BodyId fallingBody = fallingFruit.GetBox2DBodyId();
        const b2BodyId gluedBody = gluedFruit.GetBox2DBodyId();
        if (!b2Body_IsValid(fallingBody) || !b2Body_IsValid(gluedBody))
        {
            return std::nullopt;
        }

        const b2Vec2 fallingVelocity = b2Body_GetLinearVelocity(fallingBody);
        const b2Vec2 gluedVelocity = b2Body_GetLinearVelocity(gluedBody);
        const float relativeSpeedY = std::fabs(fallingVelocity.y - gluedVelocity.y);
        const float fallingMass = b2Body_GetMass(fallingBody);
        if (!std::isfinite(relativeSpeedY) || !std::isfinite(fallingMass) || fallingMass <= 0.0f)
        {
            return std::nullopt;
        }

        ImpactImpulseInfo info{};
        info.fallingBody = fallingBody;
        info.gluedBody = gluedBody;
        info.fallingMass = fallingMass;
        info.relativeSpeedY = relativeSpeedY;
        info.fallingVelocity = fallingVelocity;
        info.gluedVelocity = gluedVelocity;
        info.impulse = fallingMass * relativeSpeedY;
        if (!std::isfinite(info.impulse))
        {
            return std::nullopt;
        }

        return info;
    }
}

namespace MeteorsSpace
{
    MeteorsContacts::MeteorsContacts(Systems::PhysicsMachine* physicsMachine, bool* warnedMissingPhysicsMachine)
        : m_physicsMachine(physicsMachine)
        , m_warnedMissingPhysicsMachine(warnedMissingPhysicsMachine)
    {
        if (m_physicsMachine)
        {
            const auto& tuning = m_physicsMachine->GetTuningParameters();
            m_glueTuning.linearHertz = tuning.creamGlueLinearHertz;
            m_glueTuning.linearDampingRatio = tuning.creamGlueLinearDampingRatio;
            m_glueTuning.angularHertz = tuning.creamGlueAngularHertz;
            m_glueTuning.angularDampingRatio = tuning.creamGlueAngularDampingRatio;
            m_glueTuning.collideConnected = tuning.creamGlueCollideConnected;
            m_glueTuning.pushOutEpsilon = tuning.creamGluePushOutEpsilon;
            m_glueTuning.maxPushOut = tuning.creamGlueMaxPushOut;
            m_glueTuning.maxSpeed = tuning.creamGlueMaxSpeed;
            m_glueTuning.breakForce = tuning.creamGlueBreakForce;
            m_glueTuning.breakImpulse = tuning.creamGlueBreakImpulse;
            m_meteorRepulseImpulse = tuning.meteorRepulseImpulse;
        }
    }

    void MeteorsContacts::OnContactBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b,
        const Systems::ContactEventInfo& info)
    {
        if (!a || !b)
        {
            return;
        }

        auto* meteorsA = dynamic_cast<ActiveMeteors*>(a);
        auto* meteorsB = dynamic_cast<ActiveMeteors*>(b);
        if (!meteorsA && !meteorsB)
        {
            return;
        }

        if (meteorsA)
        {
            UpdateContactStateBegin(meteorsA, b);
        }

        if (meteorsB)
        {
            UpdateContactStateBegin(meteorsB, a);
        }

        if (meteorsA && meteorsB && m_meteorRepulseImpulse > 0.0f)
        {
            if (!meteorsA->IsPendingDestroy()
                && !meteorsB->IsPendingDestroy()
                && !IsCarriedMeteors(meteorsA)
                && !IsCarriedMeteors(meteorsB))
            {
                const b2BodyId bodyA = meteorsA->GetBox2DBodyId();
                const b2BodyId bodyB = meteorsB->GetBox2DBodyId();
                if (b2Body_IsValid(bodyA) && b2Body_IsValid(bodyB))
                {
                    const b2Vec2 posA = b2Body_GetPosition(bodyA);
                    const b2Vec2 posB = b2Body_GetPosition(bodyB);
                    const b2Vec2 delta = b2Sub(posB, posA);
                    const float distanceSquared = b2LengthSquared(delta);
                    if (std::isfinite(distanceSquared)
                        && distanceSquared > std::numeric_limits<float>::epsilon())
                    {
                        const b2Vec2 direction = b2Normalize(delta);
                        if (std::isfinite(direction.x) && std::isfinite(direction.y))
                        {
                            const b2Vec2 impulse = b2MulSV(m_meteorRepulseImpulse, direction);
                            b2Body_ApplyLinearImpulseToCenter(bodyA, b2MulSV(-1.0f, impulse), true);
                            b2Body_ApplyLinearImpulseToCenter(bodyB, impulse, true);
                        }
                    }
                }
            }
        }

        if (meteorsA && meteorsB && !IsCreamTag(meteorsA->GetTag()) && !IsCreamTag(meteorsB->GetTag()))
        {
            const bool meteorsAHasCream = m_fruitToCreams.contains(meteorsA);
            const bool meteorsBHasCream = m_fruitToCreams.contains(meteorsB);
            if (meteorsAHasCream != meteorsBHasCream && m_glueTuning.breakImpulse > 0.0f)
            {
                ActiveMeteors* gluedFruit = meteorsAHasCream ? meteorsA : meteorsB;
                ActiveMeteors* fallingFruit = meteorsAHasCream ? meteorsB : meteorsA;
                if (gluedFruit && fallingFruit)
                {
                    const auto impactInfo = TryGetImpactImpulseInfo(*fallingFruit, *gluedFruit);
                    if (impactInfo)
                    {
                        if (impactInfo->impulse > m_glueTuning.breakImpulse)
                        {
                            RemoveCreamWeldJointsForMeteors(gluedFruit, "impactBreak");
                            const b2BodyId refreshedGluedBody = gluedFruit->GetBox2DBodyId();
                            if (b2Body_IsValid(refreshedGluedBody))
                            {
                                const float gluedMass = b2Body_GetMass(refreshedGluedBody);
                                if (gluedMass > 0.0f && std::isfinite(gluedMass))
                                {
                                    const b2Vec2 relativeVelocity = b2Sub(impactInfo->fallingVelocity, impactInfo->gluedVelocity);
                                    const b2Vec2 impulseVector = b2MulSV(impactInfo->fallingMass, relativeVelocity);
                                    const b2Vec2 deltaVelocity = b2MulSV(0.8f / gluedMass, impulseVector);
                                    const b2Vec2 newVelocity = b2Add(
                                        b2Body_GetLinearVelocity(refreshedGluedBody),
                                        deltaVelocity);
                                    b2Body_SetLinearVelocity(refreshedGluedBody, newVelocity);
                                }
                            }
                            m_glueCooldown[gluedFruit] = kGlueCooldownSeconds;
                        }
                    }
                }
            }
        }

        ActiveMeteors* cream = nullptr;
        ActiveMeteors* otherMeteors = nullptr;
        if (meteorsA && meteorsB)
        {
            if (IsCreamTag(meteorsA->GetTag()))
            {
                cream = meteorsA;
                otherMeteors = meteorsB;
            }
            else if (IsCreamTag(meteorsB->GetTag()))
            {
                cream = meteorsB;
                otherMeteors = meteorsA;
            }
        }

        if (cream && otherMeteors)
        {
            if (!cream->WasEverCarried())
            {
                LogGlueReject("notActivated", cream, otherMeteors);
            }
            else
            {
                if (IsCreamTag(otherMeteors->GetTag()))
                {
                    LogGlueReject("targetIsCream", cream, otherMeteors);
                }
                else if (IsBlockedTag(otherMeteors->GetTag()))
                {
                    LogGlueReject("blockedTag", cream, otherMeteors);
                }
                else if (IsGlueCooldownActive(*otherMeteors))
                {
                    LogGlueReject("cooldown", cream, otherMeteors);
                }
                else if (m_physicsMachine)
                {
                    if (IsGlueSpeedTooFast(*cream) || IsGlueSpeedTooFast(*otherMeteors))
                    {
                        LogGlueReject("tooFast", cream, otherMeteors);
                    }
                    else
                    {
                        if (B2_IS_NULL(info.contactId))
                        {
                            LogGlueReject("missingContactId", cream, otherMeteors);
                        }
                        else if (m_pendingGlue.contains(info.contactId))
                        {
                            LogGlueReject("alreadyPending", cream, otherMeteors);
                        }
                        else
                        {
                            auto existing = m_creamWeldJoints.find(info.contactId);
                            if (existing != m_creamWeldJoints.end())
                            {
                                CleanupInvalidJoint(existing->first, existing->second);
                            }

                            if (m_creamWeldJoints.find(info.contactId) == m_creamWeldJoints.end())
                            {
                                PendingGlue pending{};
                                pending.cream = cream;
                                pending.fruit = otherMeteors;
                                pending.contactId = info.contactId;
                                pending.shapeIdA = info.shapeIdA;
                                pending.shapeIdB = info.shapeIdB;
                                m_pendingGlue.emplace(info.contactId, pending);
                            }
                            else
                            {
                                LogGlueReject("alreadyBonded", cream, otherMeteors);
                            }
                        }
                    }
                }
                else if (m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
                {
                    LogGlueReject("missingPhysicsMachine", cream, otherMeteors);
                    *m_warnedMissingPhysicsMachine = true;
                }
            }
        }

    }

    void MeteorsContacts::OnContactEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b,
        const Systems::ContactEventInfo& info)
    {
        if (!a || !b)
        {
            return;
        }

        auto* meteorsA = dynamic_cast<ActiveMeteors*>(a);
        auto* meteorsB = dynamic_cast<ActiveMeteors*>(b);
        if (!meteorsA && !meteorsB)
        {
            return;
        }

        if (meteorsA)
        {
            UpdateContactStateEnd(meteorsA, b);
        }

        if (meteorsB)
        {
            UpdateContactStateEnd(meteorsB, a);
        }

        if (!m_pendingGlue.empty() && !B2_IS_NULL(info.contactId))
        {
            m_pendingGlue.erase(info.contactId);
        }

    }

    bool MeteorsContacts::TryGetContactAnchorData(const Systems::ContactEventInfo& info, ContactAnchorData& outAnchor) const
    {
        return TryGetContactAnchorData(info.contactId, info.shapeIdA, info.shapeIdB, outAnchor);
    }

    bool MeteorsContacts::TryGetContactAnchorData(const b2ContactId& contactId, b2ShapeId shapeIdA, b2ShapeId shapeIdB,
        ContactAnchorData& outAnchor) const
    {
        const auto tryShape = [&](b2ShapeId shapeId) -> bool
        {
            if (!b2Shape_IsValid(shapeId))
            {
                return false;
            }

            const int capacity = b2Shape_GetContactCapacity(shapeId);
            if (capacity <= 0)
            {
                return false;
            }

            std::vector<b2ContactData> contacts(static_cast<size_t>(capacity));
            const int count = b2Shape_GetContactData(shapeId, contacts.data(), capacity);
            for (int i = 0; i < count; ++i)
            {
                const b2ContactData& data = contacts[static_cast<size_t>(i)];
                if (!B2_ID_EQUALS(data.contactId, contactId))
                {
                    continue;
                }

                if (data.manifold.pointCount <= 0)
                {
                    continue;
                }

                outAnchor.point = data.manifold.points[0].point;
                outAnchor.normal = data.manifold.normal;
                outAnchor.separation = data.manifold.points[0].separation;
                outAnchor.shapeIdA = data.shapeIdA;
                outAnchor.shapeIdB = data.shapeIdB;
                return true;
            }
            return false;
        };

        if (tryShape(shapeIdA))
        {
            return true;
        }

        return tryShape(shapeIdB);
    }

    bool MeteorsContacts::DoesShapeBelongToEntity(const Entities::PhysicalEntity& entity, b2ShapeId shapeId) const
    {
        if (!b2Shape_IsValid(shapeId))
        {
            return false;
        }

        b2Transform ignoredTransform{};
        std::vector<b2ShapeId> shapes;
        if (!entity.CollectBox2DShapes(shapes, ignoredTransform))
        {
            return false;
        }

        return std::any_of(shapes.begin(), shapes.end(), [&](const b2ShapeId& id)
            {
                return B2_ID_EQUALS(id, shapeId);
            });
    }

    Entities::PhysicalEntity* MeteorsContacts::GetEntityForShape(b2ShapeId shapeId) const
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

        if (void* bodyData = b2Body_GetUserData(bodyId))
        {
            return static_cast<Entities::PhysicalEntity*>(bodyData);
        }

        if (!m_physicsMachine)
        {
            return nullptr;
        }

        const uint64_t targetKey = b2StoreBodyId(bodyId);
        const auto& bodies = m_physicsMachine->GetBodies();
        for (Entities::PhysicalEntity* entity : bodies)
        {
            if (!entity)
            {
                continue;
            }

            const b2BodyId entityBodyId = entity->GetBox2DBodyId();
            if (b2Body_IsValid(entityBodyId) && b2StoreBodyId(entityBodyId) == targetKey)
            {
                return entity;
            }
        }

        return nullptr;
    }

    bool MeteorsContacts::ApplyGluePushOut(Entities::PhysicalEntity& movingBody, bool movingIsShapeA,
        const ContactAnchorData& anchorData, b2Vec2& outAnchor) const
    {
        if (!b2Body_IsValid(movingBody.GetBox2DBodyId()))
        {
            return false;
        }

        const b2BodyId bodyId = movingBody.GetBox2DBodyId();
        if (b2Body_GetType(bodyId) == b2_staticBody)
        {
            return false;
        }

        if (!std::isfinite(anchorData.separation) || anchorData.separation >= 0.0f)
        {
            return false;
        }

        const float targetPushOut = -anchorData.separation + m_glueTuning.pushOutEpsilon;
        const float pushDistance = std::min(targetPushOut, m_glueTuning.maxPushOut);
        if (pushDistance <= 0.0f)
        {
            return false;
        }

        const b2Vec2 pushNormal = movingIsShapeA
            ? b2Vec2{ -anchorData.normal.x, -anchorData.normal.y }
            : anchorData.normal;

        const b2Vec2 currentPos = b2Body_GetPosition(bodyId);
        const b2Rot currentRot = b2Body_GetRotation(bodyId);
        const b2Vec2 newPos = b2Add(currentPos, b2MulSV(pushDistance, pushNormal));
        b2Body_SetTransform(bodyId, newPos, currentRot);
        movingBody.SyncTransformFromBox2DBody();

        outAnchor = b2Add(outAnchor, b2MulSV(pushDistance, pushNormal));
        return true;
    }

    void MeteorsContacts::FreezeFruitBody(ActiveMeteors& fruit)
    {
        if (m_frozenFruitStates.contains(&fruit))
        {
            return;
        }

        const b2BodyId bodyId = fruit.GetBox2DBodyId();
        if (!b2Body_IsValid(bodyId))
        {
            return;
        }

        FrozenBodyState state{};
        state.bodyType = b2Body_GetType(bodyId);
        state.gravityScale = b2Body_GetGravityScale(bodyId);
        m_frozenFruitStates.emplace(&fruit, state);

        b2Body_SetLinearVelocity(bodyId, b2Vec2{ 0.0f, 0.0f });
        b2Body_SetAngularVelocity(bodyId, 0.0f);
        b2Body_SetGravityScale(bodyId, 0.0f);
        b2Body_SetType(bodyId, b2_staticBody);
    }

    void MeteorsContacts::UnfreezeFruitBody(ActiveMeteors& fruit)
    {
        const auto it = m_frozenFruitStates.find(&fruit);
        if (it == m_frozenFruitStates.end())
        {
            return;
        }

        const b2BodyId bodyId = fruit.GetBox2DBodyId();
        if (b2Body_IsValid(bodyId))
        {
            b2Body_SetType(bodyId, it->second.bodyType);
            b2Body_SetGravityScale(bodyId, it->second.gravityScale);
            b2Body_SetLinearVelocity(bodyId, b2Vec2{ 0.0f, 0.0f });
            b2Body_SetAngularVelocity(bodyId, 0.0f);
        }

        m_frozenFruitStates.erase(it);
    }

    bool MeteorsContacts::IsGlueSpeedTooFast(const Entities::PhysicalEntity& entity) const
    {
        if (m_glueTuning.maxSpeed <= 0.0f)
        {
            return false;
        }

        const b2BodyId bodyId = entity.GetBox2DBodyId();
        if (!b2Body_IsValid(bodyId))
        {
            return false;
        }

        const b2Vec2 velocity = b2Body_GetLinearVelocity(bodyId);
        const float speedSquared = b2LengthSquared(velocity);
        const float maxSpeedSquared = m_glueTuning.maxSpeed * m_glueTuning.maxSpeed;
        return speedSquared > maxSpeedSquared;
    }

    bool MeteorsContacts::IsGlueBreakForceExceeded(b2JointId jointId) const
    {
        if (m_glueTuning.breakForce <= 0.0f)
        {
            return false;
        }

        if (!b2Joint_IsValid(jointId))
        {
            return false;
        }

        const b2Vec2 force = b2Joint_GetConstraintForce(jointId);
        if (!std::isfinite(force.x) || !std::isfinite(force.y))
        {
            return false;
        }

        return b2Length(force) >= m_glueTuning.breakForce;
    }

    void MeteorsContacts::UpdateBreakableJoints()
    {
        if (m_glueTuning.breakForce <= 0.0f)
        {
            return;
        }

        if (!m_physicsMachine)
        {
            return;
        }

        for (auto it = m_creamWeldJoints.begin(); it != m_creamWeldJoints.end();)
        {
            auto current = it++;
            CreamWeldEntry& entry = current->second;
            if (!b2Joint_IsValid(entry.jointId))
            {
                CleanupInvalidJoint(current->first, entry);
                continue;
            }

            if (!IsGlueBreakForceExceeded(entry.jointId))
            {
                continue;
            }

            ActiveMeteors* cream = entry.cream;
            ActiveMeteors* fruit = entry.fruit;
            LogGlueDestroy("breakForce", cream, fruit);
            m_physicsMachine->DestroyJoint(entry.jointId);
            m_creamWeldJoints.erase(current->first);
            if (fruit)
            {
                RemoveFruitCreamLink(fruit, cream);
            }
        }

    }

    void MeteorsContacts::LogGlueReject(const char* reason, const ActiveMeteors* cream, const Entities::PhysicalEntity* target) const
    {
        ++m_glueStats.rejectCount;
        std::ostringstream oss;
        oss << kLogPrefix << "glue_reject_reason=" << (reason ? reason : "<unknown>")
            << " cream='" << (cream ? cream->GetTag() : "<null>")
            << "' target='" << (target ? target->GetTag() : "<null>") << "'\n";
        DebugTrace(oss.str());
    }

    void MeteorsContacts::LogGlueDestroy(const char* reason, const ActiveMeteors* cream, const Entities::PhysicalEntity* target) const
    {
        ++m_glueStats.cleanupCount;
        std::ostringstream oss;
        oss << kLogPrefix << "glue_destroy_reason=" << (reason ? reason : "<unknown>")
            << " cream='" << (cream ? cream->GetTag() : "<null>")
            << "' target='" << (target ? target->GetTag() : "<null>") << "'\n";
        DebugTrace(oss.str());
    }

    void MeteorsContacts::LogGlueCreateFruitToCream(const ActiveMeteors* cream, const ActiveMeteors* fruit,
        const b2Vec2& anchorWorld) const
    {
        ++m_glueStats.createCount;
        std::ostringstream oss;
        oss << kLogPrefix << "glue_create_fruit_to_cream cream='" << (cream ? cream->GetTag() : "<null>")
            << "' fruit='" << (fruit ? fruit->GetTag() : "<null>")
            << "' anchorWorld=(" << anchorWorld.x << "," << anchorWorld.y << ")\n";
        DebugTrace(oss.str());
    }

    void MeteorsContacts::CleanupInvalidJoint(const b2ContactId& contactId, CreamWeldEntry& entry)
    {
        if (!b2Joint_IsValid(entry.jointId))
        {
            ActiveMeteors* cream = entry.cream;
            ActiveMeteors* fruit = entry.fruit;
            LogGlueDestroy("destroyed", cream, fruit);
            m_creamWeldJoints.erase(contactId);
            if (fruit)
            {
                RemoveFruitCreamLink(fruit, cream);
            }
        }
    }

    void MeteorsContacts::RemoveFruitCreamLink(ActiveMeteors* fruit, ActiveMeteors* cream)
    {
        if (!fruit || !cream)
        {
            return;
        }

        for (const auto& entry : m_creamWeldJoints)
        {
            if (entry.second.fruit == fruit && entry.second.cream == cream)
            {
                return;
            }
        }

        auto it = m_fruitToCreams.find(fruit);
        if (it == m_fruitToCreams.end())
        {
            return;
        }

        it->second.erase(cream);
        if (it->second.empty())
        {
            m_fruitToCreams.erase(it);
            UnfreezeFruitBody(*fruit);
        }
    }

    void MeteorsContacts::RemovePendingGlueForMeteors(ActiveMeteors* meteors)
    {
        if (!meteors || m_pendingGlue.empty())
        {
            return;
        }

        for (auto it = m_pendingGlue.begin(); it != m_pendingGlue.end();)
        {
            if (it->second.cream == meteors || it->second.fruit == meteors)
            {
                it = m_pendingGlue.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    bool MeteorsContacts::HasSupportingContact(const ActiveMeteors& meteors, bool* foundationOnly) const
    {
        if (foundationOnly)
        {
            *foundationOnly = false;
        }

        auto it = m_contactStates.find(const_cast<ActiveMeteors*>(&meteors));
        if (it == m_contactStates.end())
        {
            return false;
        }

        const bool hasFoundation = it->second.foundationContacts > 0;
        const bool hasMeteorsContact = it->second.meteorsContacts > 0;
        if (hasFoundation || hasMeteorsContact)
        {
            bool hasRealContacts = false;
            const b2BodyId bodyId = meteors.GetBox2DBodyId();
            if (b2Body_IsValid(bodyId))
            {
                const int capacity = b2Body_GetContactCapacity(bodyId);
                if (capacity > 0)
                {
                    std::vector<b2ContactData> contactsBuffer(static_cast<std::size_t>(capacity));
                    const int count = b2Body_GetContactData(bodyId, contactsBuffer.data(), capacity);
                    for (int i = 0; i < count; ++i)
                    {
                        if (contactsBuffer[i].manifold.pointCount > 0)
                        {
                            hasRealContacts = true;
                            break;
                        }
                    }
                }
            }

            if (!hasRealContacts)
            {
                const_cast<MeteorsContacts*>(this)->m_contactStates.erase(it);
                return false;
            }
        }

        if (foundationOnly)
        {
            *foundationOnly = hasFoundation && !hasMeteorsContact;
        }

        return hasFoundation || hasMeteorsContact;
    }

    bool MeteorsContacts::HasEverCollidedWithMeteors(const ActiveMeteors& meteors) const
    {
        return m_hasMeteorsContact.find(const_cast<ActiveMeteors*>(&meteors)) != m_hasMeteorsContact.end();
    }

    void MeteorsContacts::ClearContactState(ActiveMeteors* meteors)
    {
        if (!meteors)
        {
            return;
        }

        m_contactStates.erase(meteors);
    }

    void MeteorsContacts::ClearMeteorsContact(ActiveMeteors* meteors)
    {
        if (!meteors)
        {
            return;
        }

        m_hasMeteorsContact.erase(meteors);
    }

    void MeteorsContacts::Reset()
    {
        m_contactStates.clear();
        m_hasMeteorsContact.clear();
        m_carriedMeteors.clear();
        m_pendingGlue.clear();
        m_glueCooldown.clear();
    }

    void MeteorsContacts::RemoveCreamWeldJointsForMeteors(ActiveMeteors* meteors, const char* context)
    {
        if (!meteors)
        {
            return;
        }
        RemovePendingGlueForMeteors(meteors);
        if (!context || std::string(context) != "impactBreak")
        {
            m_glueCooldown.erase(meteors);
        }
        if (m_creamWeldJoints.empty())
        {
            return;
        }

        const char* reason = "removeEntry";
        if (context)
        {
            const std::string contextValue = context;
            if (contextValue == "TryMarkAsCarried" || contextValue == "ReleaseCarriedMeteors")
            {
                reason = "carried";
            }
            else if (contextValue == "ClearAllMeteors")
            {
                reason = "clearAll";
            }
            else if (contextValue == "~MeteorsManager")
            {
                reason = "destroyed";
            }
        }

        if (!m_physicsMachine)
        {
            if (m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
            {
                std::ostringstream oss;
                oss << kLogPrefix << context << " でクリーム接着ジョイントを外せなかったよ 物理マシン未設定 \n";
                DebugTrace(oss.str());
                *m_warnedMissingPhysicsMachine = true;
            }
        }

        for (auto it = m_creamWeldJoints.begin(); it != m_creamWeldJoints.end();)
        {
            if (it->second.cream == meteors || it->second.fruit == meteors)
            {
                ActiveMeteors* cream = it->second.cream;
                ActiveMeteors* fruit = it->second.fruit;
                LogGlueDestroy(reason, cream, fruit);
                if (m_physicsMachine)
                {
                    m_physicsMachine->DestroyJoint(it->second.jointId);
                }
                it = m_creamWeldJoints.erase(it);
                if (fruit)
                {
                    RemoveFruitCreamLink(fruit, cream);
                }
            }
            else
            {
                ++it;
            }
        }

        if (m_frozenFruitStates.contains(meteors) && !m_fruitToCreams.contains(meteors))
        {
            UnfreezeFruitBody(*meteors);
        }
    }

    void MeteorsContacts::ClearCreamWeldJoints(const char* context)
    {
        if (m_creamWeldJoints.empty())
        {
            return;
        }

        if (!m_physicsMachine)
        {
            if (m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
            {
                std::ostringstream oss;
                oss << kLogPrefix << context << " でクリーム接着ジョイントを外せなかったよ 物理マシン未設定 \n";
                DebugTrace(oss.str());
                *m_warnedMissingPhysicsMachine = true;
            }
        }

        const char* reason = "clearAll";
        if (context && std::string(context) == "~MeteorsManager")
        {
            reason = "destroyed";
        }

        for (const auto& entry : m_creamWeldJoints)
        {
            LogGlueDestroy(reason, entry.second.cream, entry.second.fruit);
            if (m_physicsMachine)
            {
                m_physicsMachine->DestroyJoint(entry.second.jointId);
            }
        }
        m_creamWeldJoints.clear();
        m_pendingGlue.clear();
        m_fruitToCreams.clear();
        m_glueCooldown.clear();
        std::vector<ActiveMeteors*> frozenFruits;
        frozenFruits.reserve(m_frozenFruitStates.size());
        for (const auto& entry : m_frozenFruitStates)
        {
            if (entry.first)
            {
                frozenFruits.push_back(entry.first);
            }
        }
        for (ActiveMeteors* fruit : frozenFruits)
        {
            UnfreezeFruitBody(*fruit);
        }
    }

    void MeteorsContacts::ProcessPendingGlue()
    {
        if (m_pendingGlue.empty())
        {
            return;
        }

        if (!m_physicsMachine)
        {
            for (const auto& entry : m_pendingGlue)
            {
                LogGlueReject("missingPhysicsMachine", entry.second.cream, entry.second.fruit);
            }
            m_pendingGlue.clear();
            if (m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
            {
                *m_warnedMissingPhysicsMachine = true;
            }
            return;
        }

        for (auto it = m_pendingGlue.begin(); it != m_pendingGlue.end();)
        {
            PendingGlue pending = it->second;
            if (!pending.cream || !pending.fruit)
            {
                LogGlueReject("missingMeteors", pending.cream, pending.fruit);
                it = m_pendingGlue.erase(it);
                continue;
            }

            if (IsGlueCooldownActive(*pending.fruit))
            {
                LogGlueReject("cooldown", pending.cream, pending.fruit);
                it = m_pendingGlue.erase(it);
                continue;
            }

            if (IsGlueSpeedTooFast(*pending.cream) || IsGlueSpeedTooFast(*pending.fruit))
            {
                LogGlueReject("tooFast", pending.cream, pending.fruit);
                it = m_pendingGlue.erase(it);
                continue;
            }

            if (!HasSupportingContact(*pending.cream, nullptr) || !HasSupportingContact(*pending.fruit, nullptr))
            {
                LogGlueReject("noSupport", pending.cream, pending.fruit);
                it = m_pendingGlue.erase(it);
                continue;
            }

            auto existing = m_creamWeldJoints.find(pending.contactId);
            if (existing != m_creamWeldJoints.end())
            {
                CleanupInvalidJoint(existing->first, existing->second);
            }

            if (m_creamWeldJoints.find(pending.contactId) != m_creamWeldJoints.end())
            {
                LogGlueReject("alreadyBonded", pending.cream, pending.fruit);
                it = m_pendingGlue.erase(it);
                continue;
            }

            ContactAnchorData anchorData{};
            if (!TryGetContactAnchorData(pending.contactId, pending.shapeIdA, pending.shapeIdB, anchorData))
            {
                LogGlueReject("missingManifold", pending.cream, pending.fruit);
                it = m_pendingGlue.erase(it);
                continue;
            }

            b2Vec2 anchor = anchorData.point;
            const bool fruitIsShapeA = DoesShapeBelongToEntity(*pending.fruit, anchorData.shapeIdA);
            const bool fruitIsShapeB = DoesShapeBelongToEntity(*pending.fruit, anchorData.shapeIdB);
            if (fruitIsShapeA != fruitIsShapeB)
            {
                ApplyGluePushOut(*pending.fruit, fruitIsShapeA, anchorData, anchor);
            }

            const b2JointId jointId = m_physicsMachine->CreateWeldJoint(
                *pending.cream,
                *pending.fruit,
                anchor,
                m_glueTuning.linearHertz,
                m_glueTuning.linearDampingRatio,
                m_glueTuning.angularHertz,
                m_glueTuning.angularDampingRatio,
                m_glueTuning.collideConnected);
            if (b2Joint_IsValid(jointId))
            {
                CreamWeldEntry entry{};
                entry.cream = pending.cream;
                entry.fruit = pending.fruit;
                entry.jointId = jointId;
                entry.anchorWorld = anchor;
                m_creamWeldJoints.emplace(pending.contactId, entry);
                m_fruitToCreams[pending.fruit].insert(pending.cream);
                FreezeFruitBody(*pending.fruit);
                LogGlueCreateFruitToCream(pending.cream, pending.fruit, anchor);
            }
            else
            {
                LogGlueReject("createFailed", pending.cream, pending.fruit);
            }

            it = m_pendingGlue.erase(it);
        }
    }

    void MeteorsContacts::OnStateChanged(ActiveMeteors* meteors, MeteorsState previousState, MeteorsState newState)
    {
        (void)previousState;
        if (newState == MeteorsState::Carried && meteors)
        {
            m_hasMeteorsContact.erase(meteors);
            m_carriedMeteors.insert(meteors);
        }
        else if (meteors)
        {
            m_carriedMeteors.erase(meteors);
        }
    }

    void MeteorsContacts::TickDebugStats(float dt)
    {
        UpdateBreakableJoints();
        TickGlueCooldowns(dt);
        if (!std::isfinite(dt) || dt <= 0.0f)
        {
            return;
        }

        m_glueStats.elapsedSeconds += dt;
        constexpr float kLogIntervalSeconds = 2.0f;
        if (m_glueStats.elapsedSeconds < kLogIntervalSeconds)
        {
            return;
        }

        const float interval = std::max(m_glueStats.elapsedSeconds, 0.001f);
        m_glueStats.createPerSec = static_cast<float>(m_glueStats.createCount) / interval;
        m_glueStats.rejectPerSec = static_cast<float>(m_glueStats.rejectCount) / interval;
        m_glueStats.cleanupPerSec = static_cast<float>(m_glueStats.cleanupCount) / interval;
        m_glueStats.createCount = 0;
        m_glueStats.rejectCount = 0;
        m_glueStats.cleanupCount = 0;
        m_glueStats.elapsedSeconds = 0.0f;

    }

    void MeteorsContacts::TickGlueCooldowns(float dt)
    {
        if (!std::isfinite(dt) || dt <= 0.0f || m_glueCooldown.empty())
        {
            return;
        }

        for (auto it = m_glueCooldown.begin(); it != m_glueCooldown.end();)
        {
            it->second = std::max(0.0f, it->second - dt);
            if (it->second <= 0.0f)
            {
                it = m_glueCooldown.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    bool MeteorsContacts::IsGlueCooldownActive(const ActiveMeteors& fruit) const
    {
        auto it = m_glueCooldown.find(const_cast<ActiveMeteors*>(&fruit));
        if (it == m_glueCooldown.end())
        {
            return false;
        }

        return it->second > 0.0f;
    }

    bool MeteorsContacts::IsCarriedMeteors(const ActiveMeteors* meteors) const
    {
        if (!meteors)
        {
            return false;
        }

        return m_carriedMeteors.find(const_cast<ActiveMeteors*>(meteors)) != m_carriedMeteors.end();
    }

    MeteorsContacts::GlueDebugSnapshot MeteorsContacts::BuildDebugSnapshot() const
    {
        GlueDebugSnapshot snapshot{};
        snapshot.activeFruitGlue = static_cast<int>(m_creamWeldJoints.size());
        snapshot.frozenFruit = static_cast<int>(m_frozenFruitStates.size());
        snapshot.createPerSec = m_glueStats.createPerSec;
        snapshot.rejectPerSec = m_glueStats.rejectPerSec;
        snapshot.cleanupPerSec = m_glueStats.cleanupPerSec;

        snapshot.fruitLinks.reserve(m_creamWeldJoints.size());
        for (const auto& entry : m_creamWeldJoints)
        {
            GlueDebugSnapshot::GlueLink link{};
            link.anchorWorld = entry.second.anchorWorld;
            link.cream = entry.second.cream;
            link.fruit = entry.second.fruit;
            link.fruitFrozen = (entry.second.fruit && m_frozenFruitStates.contains(entry.second.fruit));
            snapshot.fruitLinks.push_back(link);
        }

        return snapshot;
    }

    void MeteorsContacts::UpdateContactStateBegin(ActiveMeteors* meteors, Entities::PhysicalEntity* other)
    {
        if (!meteors || !other)
        {
            return;
        }

        ContactState& state = m_contactStates[meteors];
        const Collision::CollisionLayer otherLayer = other->GetCollisionLayer();
        if (otherLayer == Collision::CollisionLayer::Floor)
        {
            ++state.foundationContacts;
        }
        else if (otherLayer == Collision::CollisionLayer::Meteors && dynamic_cast<ActiveMeteors*>(other))
        {
            ++state.meteorsContacts;
            m_hasMeteorsContact.insert(meteors);
        }
    }

    void MeteorsContacts::UpdateContactStateEnd(ActiveMeteors* meteors, Entities::PhysicalEntity* other)
    {
        if (!meteors || !other)
        {
            return;
        }

        auto it = m_contactStates.find(meteors);
        if (it == m_contactStates.end())
        {
            return;
        }

        ContactState& state = it->second;
        const Collision::CollisionLayer otherLayer = other->GetCollisionLayer();
        if (otherLayer == Collision::CollisionLayer::Floor && state.foundationContacts > 0)
        {
            --state.foundationContacts;
        }
        else if (otherLayer == Collision::CollisionLayer::Meteors && dynamic_cast<ActiveMeteors*>(other))
        {
            if (state.meteorsContacts > 0)
            {
                --state.meteorsContacts;
            }
        }

        if (state.foundationContacts <= 0 && state.meteorsContacts <= 0)
        {
            m_contactStates.erase(it);
        }
    }
}
