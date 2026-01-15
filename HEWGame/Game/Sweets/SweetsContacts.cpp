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
            m_glueTuning.breakForce = tuning.creamGlueBreakForce;
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
                            LogGlueCreateCreamToCake(creamForStage, otherStage, anchor);
                        }
                        else
                        {
                            LogGlueReject("createFailed", creamForStage, otherStage);
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
                    const auto fruitBindings = m_fruitToCreams.find(otherSweets);
                    if (fruitBindings != m_fruitToCreams.end() && fruitBindings->second.contains(cream))
                    {
                        LogGlueReject("alreadyBonded", cream, otherSweets);
                    }
                    else
                    {
                        const JointPairKey key = JointPairKey::Make(cream, otherSweets);
                        auto existing = m_creamWeldJoints.find(key);
                        if (existing != m_creamWeldJoints.end())
                        {
                            CleanupInvalidJoint(key, existing->second);
                        }

                        if (m_creamWeldJoints.find(key) == m_creamWeldJoints.end())
                        {
                            ContactAnchorData anchorData{};
                            if (!TryGetContactAnchorData(info, anchorData))
                            {
                                LogGlueReject("missingManifold", cream, otherSweets);
                            }
                            else
                            {
                                b2Vec2 anchor = anchorData.point;
                                const bool fruitIsShapeA = DoesShapeBelongToEntity(*otherSweets, anchorData.shapeIdA);
                                const bool fruitIsShapeB = DoesShapeBelongToEntity(*otherSweets, anchorData.shapeIdB);
                                if (fruitIsShapeA != fruitIsShapeB)
                                {
                                    ApplyGluePushOut(*otherSweets, fruitIsShapeA, anchorData, anchor);
                                }
                                const b2JointId jointId = m_physicsMachine->CreateWeldJoint(
                                    *cream,
                                    *otherSweets,
                                    anchor,
                                    m_glueTuning.linearHertz,
                                    m_glueTuning.linearDampingRatio,
                                    m_glueTuning.angularHertz,
                                    m_glueTuning.angularDampingRatio,
                                    m_glueTuning.collideConnected);
                                if (b2Joint_IsValid(jointId))
                                {
                                    m_creamWeldJoints.emplace(key, jointId);
                                    m_creamWeldAnchors.emplace(key, anchor);
                                    m_fruitToCreams[otherSweets].insert(cream);
                                    FreezeFruitBody(*otherSweets);
                                    LogGlueCreateFruitToCream(cream, otherSweets, anchor);
                                }
                                else
                                {
                                    LogGlueReject("createFailed", cream, otherSweets);
                                }
                            }
                        }
                        else
                        {
                            LogGlueReject("alreadyBonded", cream, otherSweets);
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

        (void)info;

    }

    bool SweetsContacts::TryGetContactAnchorData(const Systems::ContactEventInfo& info, ContactAnchorData& outAnchor) const
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
                if (!B2_ID_EQUALS(data.contactId, info.contactId))
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

        if (tryShape(info.shapeIdA))
        {
            return true;
        }

        return tryShape(info.shapeIdB);
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

    void SweetsContacts::CleanupInvalidJoint(const JointPairKey& key, b2JointId& jointId)
    {
        if (!b2Joint_IsValid(jointId))
        {
            ActiveSweets* cream = IsCreamTag(key.first->GetTag()) ? key.first : key.second;
            ActiveSweets* fruit = (cream == key.first) ? key.second : key.first;
            LogGlueDestroy("destroyed", cream, fruit);
            if (fruit)
            {
                RemoveFruitCreamLink(fruit, cream);
            }
            m_creamWeldJoints.erase(key);
            m_creamWeldAnchors.erase(key);
        }
    }

    void SweetsContacts::RemoveFruitCreamLink(ActiveSweets* fruit, ActiveSweets* cream)
    {
        if (!fruit || !cream)
        {
            return;
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
    }

    void SweetsContacts::RemoveCreamWeldJointsForSweets(ActiveSweets* sweets, const char* context)
    {
        if (!sweets)
        {
            return;
        }
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
            if (it->first.first == sweets || it->first.second == sweets)
            {
                ActiveSweets* cream = IsCreamTag(it->first.first->GetTag()) ? it->first.first : it->first.second;
                ActiveSweets* fruit = (cream == it->first.first) ? it->first.second : it->first.first;
                LogGlueDestroy(reason, cream, fruit);
                if (m_physicsMachine)
                {
                    m_physicsMachine->DestroyJoint(it->second);
                }
                if (fruit)
                {
                    RemoveFruitCreamLink(fruit, cream);
                }
                m_creamWeldAnchors.erase(it->first);
                it = m_creamWeldJoints.erase(it);
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
            ActiveSweets* cream = IsCreamTag(entry.first.first->GetTag()) ? entry.first.first : entry.first.second;
            ActiveSweets* fruit = (cream == entry.first.first) ? entry.first.second : entry.first.first;
            LogGlueDestroy(reason, cream, fruit);
            if (m_physicsMachine)
            {
                m_physicsMachine->DestroyJoint(entry.second);
            }
        }
        m_creamWeldJoints.clear();
        m_creamWeldAnchors.clear();
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

    void SweetsContacts::OnStateChanged(ActiveSweets* sweets, SweetsState previousState, SweetsState newState)
    {
        (void)previousState;
        if (newState == SweetsState::Carried && sweets)
        {
            m_hasSweetsContact.erase(sweets);
        }
    }

    void SweetsContacts::TickGlueBreaks(float dt)
    {
        (void)dt;
        if (!m_physicsMachine)
        {
            return;
        }

        const float breakForce = m_glueTuning.breakForce;
        const bool enableBreakForce = breakForce > 0.0f;
        for (auto it = m_creamWeldJoints.begin(); it != m_creamWeldJoints.end();)
        {
            if (!b2Joint_IsValid(it->second))
            {
                ActiveSweets* cream = IsCreamTag(it->first.first->GetTag()) ? it->first.first : it->first.second;
                ActiveSweets* fruit = (cream == it->first.first) ? it->first.second : it->first.first;
                LogGlueDestroy("destroyed", cream, fruit);
                if (fruit)
                {
                    RemoveFruitCreamLink(fruit, cream);
                }
                m_creamWeldAnchors.erase(it->first);
                it = m_creamWeldJoints.erase(it);
                continue;
            }

            if (enableBreakForce)
            {
                const b2Vec2 force = b2Joint_GetConstraintForce(it->second);
                if (b2Length(force) >= breakForce)
                {
                    ActiveSweets* cream = IsCreamTag(it->first.first->GetTag()) ? it->first.first : it->first.second;
                    ActiveSweets* fruit = (cream == it->first.first) ? it->first.second : it->first.first;
                    LogGlueDestroy("breakForce", cream, fruit);
                    m_physicsMachine->DestroyJoint(it->second);
                    if (fruit)
                    {
                        RemoveFruitCreamLink(fruit, cream);
                    }
                    m_creamWeldAnchors.erase(it->first);
                    it = m_creamWeldJoints.erase(it);
                    continue;
                }
            }

            ++it;
        }

        for (auto it = m_creamAnchorJoints.begin(); it != m_creamAnchorJoints.end();)
        {
            if (!b2Joint_IsValid(it->second))
            {
                LogGlueDestroy("destroyed", it->first, nullptr);
                m_creamAnchorPoints.erase(it->first);
                it = m_creamAnchorJoints.erase(it);
                continue;
            }

            if (enableBreakForce)
            {
                const b2Vec2 force = b2Joint_GetConstraintForce(it->second);
                if (b2Length(force) >= breakForce)
                {
                    LogGlueDestroy("breakForce", it->first, nullptr);
                    m_physicsMachine->DestroyJoint(it->second);
                    m_creamAnchorPoints.erase(it->first);
                    it = m_creamAnchorJoints.erase(it);
                    continue;
                }
            }

            ++it;
        }
    }

    void SweetsContacts::TickDebugStats(float dt)
    {
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

        snapshot.fruitLinks.reserve(m_creamWeldAnchors.size());
        for (const auto& entry : m_creamWeldAnchors)
        {
            GlueDebugSnapshot::GlueLink link{};
            ActiveSweets* cream = IsCreamTag(entry.first.first->GetTag()) ? entry.first.first : entry.first.second;
            ActiveSweets* fruit = (cream == entry.first.first) ? entry.first.second : entry.first.first;
            link.anchorWorld = entry.second;
            link.cream = cream;
            link.fruit = fruit;
            link.fruitFrozen = (fruit && m_frozenFruitStates.contains(fruit));
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
