#include "Sweets/SweetsContacts.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

#include "CollisionLayer.h"
#include "Sweets/BaseSweets.h"
#include "Sweets/SweetsKeyUtils.h"
#include "SweetsPrototypeManifest.h"
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

    constexpr const char* kLogPrefix = "[SweetsManager] ";

    std::string NormalizeTag(const std::string& tag)
    {
        return SweetsSpace::NormalizeKey(tag);
    }

    bool IsCreamTag(const std::string& tag)
    {
        static const std::string kCreamKey = NormalizeTag(::SweetsSpace::ToString(::SweetsSpace::SweetsType::Cream));
        return NormalizeTag(tag) == kCreamKey;
    }

    bool IsCakeTag(const std::string& tag)
    {
        static const std::string kCakeKey = NormalizeTag("cake");
        return NormalizeTag(tag) == kCakeKey;
    }

    bool IsBlockedTag(const std::string& tag)
    {
        static const std::unordered_set<std::string> kBlockedTags = {
            NormalizeTag("osara"),
            NormalizeTag("fairy"),
            NormalizeTag("player"),
        };
        return kBlockedTags.contains(NormalizeTag(tag));
    }
}

namespace SweetsSpace
{
    SweetsContacts::SweetsContacts(Systems::PhysicsMachine* physicsMachine, bool* warnedMissingPhysicsMachine)
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
        }
    }

    void SweetsContacts::OnContactBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b,
        const Systems::ContactEventInfo& info)
    {
        if (!a || !b)
        {
            return;
        }

        auto* sweetsA = dynamic_cast<ActiveSweets*>(a);
        auto* sweetsB = dynamic_cast<ActiveSweets*>(b);
        if (!sweetsA && !sweetsB)
        {
            return;
        }

        if (sweetsA)
        {
            UpdateContactStateBegin(sweetsA, b);
        }

        if (sweetsB)
        {
            UpdateContactStateBegin(sweetsB, a);
        }

        ActiveSweets* cream = nullptr;
        ActiveSweets* otherSweets = nullptr;
        if (sweetsA && sweetsB)
        {
            if (IsCreamTag(sweetsA->GetTag()))
            {
                cream = sweetsA;
                otherSweets = sweetsB;
            }
            else if (IsCreamTag(sweetsB->GetTag()))
            {
                cream = sweetsB;
                otherSweets = sweetsA;
            }
        }

        ActiveSweets* creamForStage = nullptr;
        Entities::PhysicalEntity* otherStage = nullptr;
        if (sweetsA && IsCreamTag(sweetsA->GetTag()) && !sweetsB)
        {
            creamForStage = sweetsA;
            otherStage = b;
        }
        else if (sweetsB && IsCreamTag(sweetsB->GetTag()) && !sweetsA)
        {
            creamForStage = sweetsB;
            otherStage = a;
        }

        if (creamForStage && otherStage && IsCakeTag(otherStage->GetTag()))
        {
            if (!creamForStage->WasEverCarried())
            {
                LogGlueReject("notActivated", creamForStage, otherStage);
            }
            else if (m_physicsMachine)
            {
                if (IsGlueSpeedTooFast(*creamForStage) || IsGlueSpeedTooFast(*otherStage))
                {
                    LogGlueReject("tooFast", creamForStage, otherStage);
                }
                else
                {
                    CleanupInvalidAnchor(creamForStage);
                    if (m_creamAnchorJoints.contains(creamForStage))
                    {
                        LogGlueReject("alreadyBonded", creamForStage, otherStage);
                    }
                    else
                    {
                        ContactAnchorData anchorData{};
                        if (!TryGetContactAnchorData(info, anchorData))
                        {
                            LogGlueReject("missingManifold", creamForStage, otherStage);
                        }
                        else
                        {
                            b2Vec2 anchor = anchorData.point;
                            const bool creamIsShapeA = DoesShapeBelongToEntity(*creamForStage, anchorData.shapeIdA);
                            const bool creamIsShapeB = DoesShapeBelongToEntity(*creamForStage, anchorData.shapeIdB);
                            if (creamIsShapeA != creamIsShapeB)
                            {
                                ApplyGluePushOut(*creamForStage, creamIsShapeA, anchorData, anchor);
                            }
                            const b2JointId jointId = m_physicsMachine->CreateWeldJoint(
                                *creamForStage,
                                *otherStage,
                                anchor,
                                m_glueTuning.linearHertz,
                                m_glueTuning.linearDampingRatio,
                                m_glueTuning.angularHertz,
                                m_glueTuning.angularDampingRatio,
                                m_glueTuning.collideConnected);
                            if (b2Joint_IsValid(jointId))
                            {
                                m_creamAnchorJoints.emplace(creamForStage, jointId);
                                m_creamAnchorPoints.emplace(creamForStage, anchor);
                                if (!B2_IS_NULL(info.contactId))
                                {
                                    m_creamAnchorContacts[creamForStage] = info.contactId;
                                }
                                LogGlueCreateCreamToCake(creamForStage, otherStage, anchor);
                            }
                            else
                            {
                                LogGlueReject("createFailed", creamForStage, otherStage);
                            }
                        }
                    }
                }
            }
            else if (m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
            {
                LogGlueReject("missingPhysicsMachine", creamForStage, otherStage);
                *m_warnedMissingPhysicsMachine = true;
            }
        }
        else if (creamForStage && otherStage)
        {
            LogGlueReject(IsBlockedTag(otherStage->GetTag()) ? "blockedTag" : "targetNotAllowed", creamForStage, otherStage);
        }

        if (cream && otherSweets)
        {
            if (!cream->WasEverCarried())
            {
                LogGlueReject("notActivated", cream, otherSweets);
            }
            else
            {
                CleanupInvalidAnchor(cream);
                if (IsCreamTag(otherSweets->GetTag()))
                {
                    LogGlueReject("targetIsCream", cream, otherSweets);
                }
                else if (IsBlockedTag(otherSweets->GetTag()))
                {
                    LogGlueReject("blockedTag", cream, otherSweets);
                }
                else if (m_physicsMachine)
                {
                    if (IsGlueSpeedTooFast(*cream) || IsGlueSpeedTooFast(*otherSweets))
                    {
                        LogGlueReject("tooFast", cream, otherSweets);
                    }
                    else
                    {
                        if (B2_IS_NULL(info.contactId))
                        {
                            LogGlueReject("missingContactId", cream, otherSweets);
                        }
                        else if (m_pendingGlue.contains(info.contactId))
                        {
                            LogGlueReject("alreadyPending", cream, otherSweets);
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
                                pending.fruit = otherSweets;
                                pending.contactId = info.contactId;
                                pending.shapeIdA = info.shapeIdA;
                                pending.shapeIdB = info.shapeIdB;
                                m_pendingGlue.emplace(info.contactId, pending);
                            }
                            else
                            {
                                LogGlueReject("alreadyBonded", cream, otherSweets);
                            }
                        }
                    }
                }
                else if (m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
                {
                    LogGlueReject("missingPhysicsMachine", cream, otherSweets);
                    *m_warnedMissingPhysicsMachine = true;
                }
            }
        }

    }

    void SweetsContacts::OnContactEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b,
        const Systems::ContactEventInfo& info)
    {
        if (!a || !b)
        {
            return;
        }

        auto* sweetsA = dynamic_cast<ActiveSweets*>(a);
        auto* sweetsB = dynamic_cast<ActiveSweets*>(b);
        if (!sweetsA && !sweetsB)
        {
            return;
        }

        if (sweetsA)
        {
            UpdateContactStateEnd(sweetsA, b);
        }

        if (sweetsB)
        {
            UpdateContactStateEnd(sweetsB, a);
        }

        if (!m_pendingGlue.empty() && !B2_IS_NULL(info.contactId))
        {
            m_pendingGlue.erase(info.contactId);
        }

    }

    bool SweetsContacts::TryGetContactAnchorData(const Systems::ContactEventInfo& info, ContactAnchorData& outAnchor) const
    {
        return TryGetContactAnchorData(info.contactId, info.shapeIdA, info.shapeIdB, outAnchor);
    }

    bool SweetsContacts::TryGetContactAnchorData(const b2ContactId& contactId, b2ShapeId shapeIdA, b2ShapeId shapeIdB,
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

    bool SweetsContacts::DoesShapeBelongToEntity(const Entities::PhysicalEntity& entity, b2ShapeId shapeId) const
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

    bool SweetsContacts::ApplyGluePushOut(Entities::PhysicalEntity& movingBody, bool movingIsShapeA,
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

    void SweetsContacts::FreezeFruitBody(ActiveSweets& fruit)
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

    void SweetsContacts::UnfreezeFruitBody(ActiveSweets& fruit)
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

    bool SweetsContacts::IsGlueSpeedTooFast(const Entities::PhysicalEntity& entity) const
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

    bool SweetsContacts::IsGlueBreakForceExceeded(b2JointId jointId) const
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

    bool SweetsContacts::IsGlueBreakImpulseExceeded(const b2ContactId& contactId) const
    {
        if (m_glueTuning.breakImpulse <= 0.0f)
        {
            return false;
        }

        if (B2_IS_NULL(contactId) || !b2Contact_IsValid(contactId))
        {
            return false;
        }

        const b2ContactData data = b2Contact_GetData(contactId);
        if (data.manifold.pointCount <= 0)
        {
            return false;
        }

        float maxImpulse = 0.0f;
        const int pointCount = std::min(data.manifold.pointCount, 2);
        for (int i = 0; i < pointCount; ++i)
        {
            const b2ManifoldPoint& point = data.manifold.points[i];
            if (!std::isfinite(point.totalNormalImpulse) || !std::isfinite(point.tangentImpulse))
            {
                continue;
            }

            const float impulse = std::hypot(point.totalNormalImpulse, point.tangentImpulse);
            maxImpulse = std::max(maxImpulse, impulse);
        }

        return maxImpulse >= m_glueTuning.breakImpulse;
    }

    void SweetsContacts::UpdateBreakableJoints()
    {
        if (m_glueTuning.breakForce <= 0.0f && m_glueTuning.breakImpulse <= 0.0f)
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

            ActiveSweets* cream = entry.cream;
            ActiveSweets* fruit = entry.fruit;
            const bool breakForce = IsGlueBreakForceExceeded(entry.jointId);
            const bool breakImpulse = IsGlueBreakImpulseExceeded(current->first);
            if (!breakForce && !breakImpulse)
            {
                continue;
            }

            LogGlueDestroy(breakImpulse ? "breakImpulse" : "breakForce", cream, fruit);
            m_physicsMachine->DestroyJoint(entry.jointId);
            m_creamWeldJoints.erase(current->first);
            if (fruit)
            {
                RemoveFruitCreamLink(fruit, cream);
            }
        }

        for (auto it = m_creamAnchorJoints.begin(); it != m_creamAnchorJoints.end();)
        {
            auto current = it++;
            b2JointId jointId = current->second;
            if (!b2Joint_IsValid(jointId))
            {
                LogGlueDestroy("destroyed", current->first, nullptr);
                m_creamAnchorPoints.erase(current->first);
                m_creamAnchorContacts.erase(current->first);
                m_creamAnchorJoints.erase(current);
                continue;
            }

            const bool breakForce = IsGlueBreakForceExceeded(jointId);
            const auto contactIt = m_creamAnchorContacts.find(current->first);
            const bool breakImpulse = contactIt != m_creamAnchorContacts.end()
                ? IsGlueBreakImpulseExceeded(contactIt->second)
                : false;
            if (!breakForce && !breakImpulse)
            {
                continue;
            }

            LogGlueDestroy(breakImpulse ? "breakImpulse" : "breakForce", current->first, nullptr);
            m_physicsMachine->DestroyJoint(jointId);
            m_creamAnchorPoints.erase(current->first);
            m_creamAnchorContacts.erase(current->first);
            m_creamAnchorJoints.erase(current);
        }
    }

    void SweetsContacts::LogGlueReject(const char* reason, const ActiveSweets* cream, const Entities::PhysicalEntity* target) const
    {
        ++m_glueStats.rejectCount;
        std::ostringstream oss;
        oss << kLogPrefix << "glue_reject_reason=" << (reason ? reason : "<unknown>")
            << " cream='" << (cream ? cream->GetTag() : "<null>")
            << "' target='" << (target ? target->GetTag() : "<null>") << "'\n";
        DebugTrace(oss.str());
    }

    void SweetsContacts::LogGlueDestroy(const char* reason, const ActiveSweets* cream, const Entities::PhysicalEntity* target) const
    {
        ++m_glueStats.cleanupCount;
        std::ostringstream oss;
        oss << kLogPrefix << "glue_destroy_reason=" << (reason ? reason : "<unknown>")
            << " cream='" << (cream ? cream->GetTag() : "<null>")
            << "' target='" << (target ? target->GetTag() : "<null>") << "'\n";
        DebugTrace(oss.str());
    }

    void SweetsContacts::LogGlueCreateCreamToCake(const ActiveSweets* cream, const Entities::PhysicalEntity* cake,
        const b2Vec2& anchorWorld) const
    {
        ++m_glueStats.createCount;
        std::ostringstream oss;
        oss << kLogPrefix << "glue_create_cream_to_cake cream='" << (cream ? cream->GetTag() : "<null>")
            << "' cake='" << (cake ? cake->GetTag() : "<null>")
            << "' anchorWorld=(" << anchorWorld.x << "," << anchorWorld.y << ")\n";
        DebugTrace(oss.str());
    }

    void SweetsContacts::LogGlueCreateFruitToCream(const ActiveSweets* cream, const ActiveSweets* fruit,
        const b2Vec2& anchorWorld) const
    {
        ++m_glueStats.createCount;
        std::ostringstream oss;
        oss << kLogPrefix << "glue_create_fruit_to_cream cream='" << (cream ? cream->GetTag() : "<null>")
            << "' fruit='" << (fruit ? fruit->GetTag() : "<null>")
            << "' anchorWorld=(" << anchorWorld.x << "," << anchorWorld.y << ")\n";
        DebugTrace(oss.str());
    }

    void SweetsContacts::CleanupInvalidJoint(const b2ContactId& contactId, CreamWeldEntry& entry)
    {
        if (!b2Joint_IsValid(entry.jointId))
        {
            ActiveSweets* cream = entry.cream;
            ActiveSweets* fruit = entry.fruit;
            LogGlueDestroy("destroyed", cream, fruit);
            m_creamWeldJoints.erase(contactId);
            if (fruit)
            {
                RemoveFruitCreamLink(fruit, cream);
            }
        }
    }

    void SweetsContacts::RemoveFruitCreamLink(ActiveSweets* fruit, ActiveSweets* cream)
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

    void SweetsContacts::RemovePendingGlueForSweets(ActiveSweets* sweets)
    {
        if (!sweets || m_pendingGlue.empty())
        {
            return;
        }

        for (auto it = m_pendingGlue.begin(); it != m_pendingGlue.end();)
        {
            if (it->second.cream == sweets || it->second.fruit == sweets)
            {
                it = m_pendingGlue.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void SweetsContacts::CleanupInvalidAnchor(ActiveSweets* cream)
    {
        if (!cream)
        {
            return;
        }

        auto it = m_creamAnchorJoints.find(cream);
        if (it != m_creamAnchorJoints.end() && !b2Joint_IsValid(it->second))
        {
            LogGlueDestroy("destroyed", cream, nullptr);
            m_creamAnchorJoints.erase(it);
            m_creamAnchorPoints.erase(cream);
            m_creamAnchorContacts.erase(cream);
        }
    }

    bool SweetsContacts::HasSupportingContact(const ActiveSweets& sweets, bool* foundationOnly) const
    {
        if (foundationOnly)
        {
            *foundationOnly = false;
        }

        auto it = m_contactStates.find(const_cast<ActiveSweets*>(&sweets));
        if (it == m_contactStates.end())
        {
            return false;
        }

        const bool hasFoundation = it->second.foundationContacts > 0;
        const bool hasSweetsContact = it->second.sweetsContacts > 0;
        if (foundationOnly)
        {
            *foundationOnly = hasFoundation && !hasSweetsContact;
        }

        return hasFoundation || hasSweetsContact;
    }

    bool SweetsContacts::HasEverCollidedWithSweets(const ActiveSweets& sweets) const
    {
        return m_hasSweetsContact.find(const_cast<ActiveSweets*>(&sweets)) != m_hasSweetsContact.end();
    }

    void SweetsContacts::ClearContactState(ActiveSweets* sweets)
    {
        if (!sweets)
        {
            return;
        }

        m_contactStates.erase(sweets);
    }

    void SweetsContacts::ClearSweetsContact(ActiveSweets* sweets)
    {
        if (!sweets)
        {
            return;
        }

        m_hasSweetsContact.erase(sweets);
    }

    void SweetsContacts::Reset()
    {
        m_contactStates.clear();
        m_hasSweetsContact.clear();
        m_pendingGlue.clear();
    }

    void SweetsContacts::RemoveCreamWeldJointsForSweets(ActiveSweets* sweets, const char* context)
    {
        if (!sweets)
        {
            return;
        }
        RemovePendingGlueForSweets(sweets);
        if (m_creamWeldJoints.empty() && m_creamAnchorJoints.empty())
        {
            return;
        }

        const char* reason = "removeEntry";
        if (context)
        {
            const std::string contextValue = context;
            if (contextValue == "TryMarkAsCarried" || contextValue == "ReleaseCarriedSweets")
            {
                reason = "carried";
            }
            else if (contextValue == "ClearAllSweets")
            {
                reason = "clearAll";
            }
            else if (contextValue == "~SweetsManager")
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
            if (it->second.cream == sweets || it->second.fruit == sweets)
            {
                ActiveSweets* cream = it->second.cream;
                ActiveSweets* fruit = it->second.fruit;
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

        auto anchorIt = m_creamAnchorJoints.find(sweets);
        if (anchorIt != m_creamAnchorJoints.end())
        {
            LogGlueDestroy(reason, sweets, nullptr);
            if (m_physicsMachine)
            {
                m_physicsMachine->DestroyJoint(anchorIt->second);
            }
            m_creamAnchorJoints.erase(anchorIt);
            m_creamAnchorPoints.erase(sweets);
            m_creamAnchorContacts.erase(sweets);
        }

        if (m_frozenFruitStates.contains(sweets) && !m_fruitToCreams.contains(sweets))
        {
            UnfreezeFruitBody(*sweets);
        }
    }

    void SweetsContacts::ClearCreamWeldJoints(const char* context)
    {
        if (m_creamWeldJoints.empty() && m_creamAnchorJoints.empty())
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
        if (context && std::string(context) == "~SweetsManager")
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

        for (const auto& entry : m_creamAnchorJoints)
        {
            LogGlueDestroy(reason, entry.first, nullptr);
            if (m_physicsMachine)
            {
                m_physicsMachine->DestroyJoint(entry.second);
            }
        }
        m_creamAnchorJoints.clear();
        m_creamAnchorPoints.clear();
        m_creamAnchorContacts.clear();
        std::vector<ActiveSweets*> frozenFruits;
        frozenFruits.reserve(m_frozenFruitStates.size());
        for (const auto& entry : m_frozenFruitStates)
        {
            if (entry.first)
            {
                frozenFruits.push_back(entry.first);
            }
        }
        for (ActiveSweets* fruit : frozenFruits)
        {
            UnfreezeFruitBody(*fruit);
        }
    }

    void SweetsContacts::ProcessPendingGlue()
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
                LogGlueReject("missingSweets", pending.cream, pending.fruit);
                it = m_pendingGlue.erase(it);
                continue;
            }

            if (IsGlueSpeedTooFast(*pending.cream) || IsGlueSpeedTooFast(*pending.fruit))
            {
                LogGlueReject("tooFast", pending.cream, pending.fruit);
                it = m_pendingGlue.erase(it);
                continue;
            }

            CleanupInvalidAnchor(pending.cream);

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

    void SweetsContacts::OnStateChanged(ActiveSweets* sweets, SweetsState previousState, SweetsState newState)
    {
        (void)previousState;
        if (newState == SweetsState::Carried && sweets)
        {
            m_hasSweetsContact.erase(sweets);
        }
    }

    void SweetsContacts::TickDebugStats(float dt)
    {
        UpdateBreakableJoints();
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

    SweetsContacts::GlueDebugSnapshot SweetsContacts::BuildDebugSnapshot() const
    {
        GlueDebugSnapshot snapshot{};
        snapshot.activeCreamAnchors = static_cast<int>(m_creamAnchorJoints.size());
        snapshot.activeFruitGlue = static_cast<int>(m_creamWeldJoints.size());
        snapshot.frozenFruit = static_cast<int>(m_frozenFruitStates.size());
        snapshot.createPerSec = m_glueStats.createPerSec;
        snapshot.rejectPerSec = m_glueStats.rejectPerSec;
        snapshot.cleanupPerSec = m_glueStats.cleanupPerSec;

        snapshot.creamAnchors.reserve(m_creamAnchorPoints.size());
        for (const auto& entry : m_creamAnchorPoints)
        {
            snapshot.creamAnchors.push_back(entry.second);
        }

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

    void SweetsContacts::UpdateContactStateBegin(ActiveSweets* sweets, Entities::PhysicalEntity* other)
    {
        if (!sweets || !other)
        {
            return;
        }

        ContactState& state = m_contactStates[sweets];
        const Collision::CollisionLayer otherLayer = other->GetCollisionLayer();
        if (otherLayer == Collision::CollisionLayer::Floor)
        {
            ++state.foundationContacts;
        }
        else if (otherLayer == Collision::CollisionLayer::Sweets && dynamic_cast<ActiveSweets*>(other))
        {
            ++state.sweetsContacts;
            m_hasSweetsContact.insert(sweets);
        }
    }

    void SweetsContacts::UpdateContactStateEnd(ActiveSweets* sweets, Entities::PhysicalEntity* other)
    {
        if (!sweets || !other)
        {
            return;
        }

        auto it = m_contactStates.find(sweets);
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
        else if (otherLayer == Collision::CollisionLayer::Sweets && dynamic_cast<ActiveSweets*>(other))
        {
            if (state.sweetsContacts > 0)
            {
                --state.sweetsContacts;
            }
        }

        if (state.foundationContacts <= 0 && state.sweetsContacts <= 0)
        {
            m_contactStates.erase(it);
        }
    }
}
