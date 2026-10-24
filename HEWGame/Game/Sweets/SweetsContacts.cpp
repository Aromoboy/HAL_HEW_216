#include "Sweets/SweetsContacts.h"

#include <algorithm>
#include <cmath>
#include <deque>
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
    constexpr float kGlueCooldownSeconds = 2.0f;

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
        m_cakeAnchorsDirty = true;
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

        if (!m_creamEnabled)
        {
            return;
        }

        if (sweetsA && sweetsB && !IsCreamTag(sweetsA->GetTag()) && !IsCreamTag(sweetsB->GetTag()))
        {
            const bool sweetsAHasCream = m_fruitToCreams.contains(sweetsA);
            const bool sweetsBHasCream = m_fruitToCreams.contains(sweetsB);
            if (sweetsAHasCream != sweetsBHasCream && m_glueTuning.breakImpulse > 0.0f)
            {
                ActiveSweets* gluedFruit = sweetsAHasCream ? sweetsA : sweetsB;
                const float verticalImpulse = std::fabs(info.manifold.normal.y) * info.normalImpulse;
                if (info.hasManifold && std::isfinite(verticalImpulse)
                    && verticalImpulse > m_glueTuning.breakImpulse)
                {
                    // ミカ：動的ボディは衝突インパルスを既に受けているため追加で加速しない
                    RemoveCreamWeldJointsForSweets(gluedFruit, "impactBreak");
                    m_glueCooldown[gluedFruit] = kGlueCooldownSeconds;
                }
            }
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
            QueueGlue(creamForStage, nullptr, otherStage, info);
        }
        else if (creamForStage && otherStage)
        {
            LogGlueReject(IsBlockedTag(otherStage->GetTag()) ? "blockedTag" : "targetNotAllowed", creamForStage, otherStage);
        }

        if (cream && otherSweets)
        {
            if (IsCreamTag(otherSweets->GetTag()))
            {
                LogGlueReject("targetIsCream", cream, otherSweets);
            }
            else if (IsBlockedTag(otherSweets->GetTag()))
            {
                LogGlueReject("blockedTag", cream, otherSweets);
            }
            else
            {
                QueueGlue(cream, otherSweets, nullptr, info);
            }
        }
    }

    void SweetsContacts::QueueGlue(ActiveSweets* cream, ActiveSweets* fruit,
        Entities::PhysicalEntity* cake, const Systems::ContactEventInfo& info)
    {
        Entities::PhysicalEntity* target = fruit ? fruit : cake;
        if (!cream->WasEverCarried())
        {
            LogGlueReject("notActivated", cream, target);
            return;
        }
        if (B2_IS_NULL(info.contactId))
        {
            LogGlueReject("missingContactId", cream, target);
            return;
        }

        PendingGlue pending{};
        pending.cream = cream;
        pending.fruit = fruit;
        pending.cake = cake;
        pending.contactId = info.contactId;
        pending.shapeIdA = info.shapeIdA;
        pending.shapeIdB = info.shapeIdB;
        m_pendingGlue.try_emplace(info.contactId, pending);
    }

    void SweetsContacts::OnContactEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b,
        const Systems::ContactEventInfo& info)
    {
        m_cakeAnchorsDirty = true;
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

    void SweetsContacts::SetCreamEnabled(bool enabled)
    {
        if (m_creamEnabled == enabled)
        {
            return;
        }
        m_creamEnabled = enabled;
        if (!m_creamEnabled)
        {
            ClearCreamWeldJoints("creamDisabled");
        }
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

            m_contactScratch.resize(static_cast<size_t>(capacity));
            const int count = b2Shape_GetContactData(shapeId, m_contactScratch.data(), capacity);
            for (int i = 0; i < count; ++i)
            {
                const b2ContactData& data = m_contactScratch[static_cast<size_t>(i)];
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

    Entities::PhysicalEntity* SweetsContacts::GetEntityForShape(b2ShapeId shapeId) const
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

    void SweetsContacts::UpdateBreakableJoints()
    {
        // ミカ：力による破壊が無効でも消失したジョイントの登録は片付ける
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

            ActiveSweets* cream = entry.cream;
            ActiveSweets* fruit = entry.fruit;
            LogGlueDestroy("breakForce", cream, fruit);
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
                m_creamAnchorJoints.erase(current);
                continue;
            }

            if (!IsGlueBreakForceExceeded(jointId))
            {
                continue;
            }

            LogGlueDestroy("breakForce", current->first, nullptr);
            m_physicsMachine->DestroyJoint(jointId);
            m_creamAnchorPoints.erase(current->first);
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

        auto it = m_fruitToCreams.find(fruit);
        if (it == m_fruitToCreams.end())
        {
            return;
        }

        auto creamIt = it->second.find(cream);
        if (creamIt == it->second.end())
        {
            return;
        }

        // ミカ：全ジョイントを再走査せず、削除した接着一つ分だけ減らす
        if (creamIt->second > 1)
        {
            --creamIt->second;
            return;
        }
        it->second.erase(creamIt);
        if (it->second.empty())
        {
            m_fruitToCreams.erase(it);
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
        if (hasFoundation || hasSweetsContact)
        {
            if (!HasAnyContact(sweets))
            {
                const_cast<SweetsContacts*>(this)->m_contactStates.erase(it);
                return false;
            }
        }

        if (foundationOnly)
        {
            *foundationOnly = hasFoundation && !hasSweetsContact;
        }

        return hasFoundation || hasSweetsContact;
    }

    bool SweetsContacts::HasAnyContact(const ActiveSweets& sweets) const
    {
        const b2BodyId bodyId = sweets.GetBox2DBodyId();
        if (!b2Body_IsValid(bodyId))
        {
            return false;
        }
        const int capacity = b2Body_GetContactCapacity(bodyId);
        if (capacity <= 0)
        {
            return false;
        }
        // ミカ：接触の一時領域を再利用し、静止したフルーツごとの確保を避ける
        m_contactScratch.resize(static_cast<std::size_t>(capacity));
        const int count = b2Body_GetContactData(bodyId, m_contactScratch.data(), capacity);
        for (int i = 0; i < count; ++i)
        {
            if (m_contactScratch[static_cast<std::size_t>(i)].manifold.pointCount > 0)
            {
                return true;
            }
        }
        return false;
    }

    bool SweetsContacts::HasEverCollidedWithSweets(const ActiveSweets& sweets) const
    {
        return m_hasSweetsContact.find(const_cast<ActiveSweets*>(&sweets)) != m_hasSweetsContact.end();
    }

    bool SweetsContacts::IsAnchoredToCake(const ActiveSweets& sweets) const
    {
        return GetCakeAnchoredSweets().contains(const_cast<ActiveSweets*>(&sweets));
    }

    const std::unordered_set<ActiveSweets*>& SweetsContacts::GetCakeAnchoredSweets() const
    {
        const std::uint64_t revision = m_physicsMachine ? m_physicsMachine->GetContactRevision() : 0;
        if (!m_cakeAnchorsDirty && m_cachedContactRevision == revision)
        {
            return m_cakeAnchoredSweets;
        }
        // ミカ：同じ物理状態の接続結果を共有し、次の探索でもグラフの領域を再利用する
        for (auto& entry : m_contactGraph)
        {
            entry.second.neighbors.clear();
            entry.second.visited = false;
        }
        m_contactQueue.clear();
        const auto addAnchor = [&](ActiveSweets* sweets)
        {
            if (sweets && !sweets->IsPendingDestroy())
            {
                auto& node = m_contactGraph[sweets];
                if (!node.visited)
                {
                    node.visited = true;
                    m_cakeAnchoredSweets.insert(sweets);
                    m_contactQueue.push_back(sweets);
                }
            }
        };

        for (const auto& entry : m_creamAnchorJoints)
        {
            if (b2Joint_IsValid(entry.second))
            {
                addAnchor(entry.first);
            }
        }
        for (const auto& entry : m_creamWeldJoints)
        {
            ActiveSweets* cream = entry.second.cream;
            ActiveSweets* fruit = entry.second.fruit;
            if (cream && fruit && !cream->IsPendingDestroy() && !fruit->IsPendingDestroy()
                && b2Joint_IsValid(entry.second.jointId))
            {
                m_contactGraph[cream].neighbors.push_back(fruit);
                m_contactGraph[fruit].neighbors.push_back(cream);
            }
        }

        // ミカ：各ボディの接触を一度だけ読み、ケーキから接続全体を探索する
        if (m_physicsMachine)
        {
            for (Entities::PhysicalEntity* entity : m_physicsMachine->GetBodies())
            {
                auto* sweets = dynamic_cast<ActiveSweets*>(entity);
                if (!sweets || sweets->IsPendingDestroy())
                {
                    continue;
                }
                const b2BodyId bodyId = sweets->GetBox2DBodyId();
                if (!b2Body_IsValid(bodyId))
                {
                    continue;
                }
                const int capacity = b2Body_GetContactCapacity(bodyId);
                if (capacity <= 0)
                {
                    continue;
                }
                m_contactScratch.resize(static_cast<std::size_t>(capacity));
                const int count = b2Body_GetContactData(bodyId, m_contactScratch.data(), capacity);
                for (int i = 0; i < count; ++i)
                {
                    const b2ContactData& data = m_contactScratch[static_cast<std::size_t>(i)];
                    if (data.manifold.pointCount <= 0
                        || !b2Shape_IsValid(data.shapeIdA) || !b2Shape_IsValid(data.shapeIdB))
                    {
                        continue;
                    }
                    const bool isShapeA = B2_ID_EQUALS(b2Shape_GetBody(data.shapeIdA), bodyId);
                    const b2ShapeId otherShape = isShapeA ? data.shapeIdB : data.shapeIdA;
                    Entities::PhysicalEntity* other = GetEntityForShape(otherShape);
                    if (!other || other == sweets)
                    {
                        continue;
                    }
                    if (IsCakeTag(other->GetTag()))
                    {
                        addAnchor(sweets);
                    }
                    else if (auto* otherSweets = dynamic_cast<ActiveSweets*>(other))
                    {
                        if (!otherSweets->IsPendingDestroy())
                        {
                            m_contactGraph[sweets].neighbors.push_back(otherSweets);
                        }
                    }
                }
            }
        }

        for (std::size_t next = 0; next < m_contactQueue.size(); ++next)
        {
            ActiveSweets* current = m_contactQueue[next];
            const auto found = m_contactGraph.find(current);
            if (found != m_contactGraph.end())
            {
                for (ActiveSweets* neighbor : found->second.neighbors)
                {
                    addAnchor(neighbor);
                }
            }
        }
        std::erase_if(m_cakeAnchoredSweets, [&](ActiveSweets* sweets)
        {
            const auto found = m_contactGraph.find(sweets);
            return found == m_contactGraph.end() || !found->second.visited;
        });
        std::erase_if(m_contactGraph, [](const auto& entry)
        {
            return !entry.second.visited && entry.second.neighbors.empty();
        });
        m_cachedContactRevision = revision;
        m_cakeAnchorsDirty = false;
        return m_cakeAnchoredSweets;
    }

    void SweetsContacts::ClearContactState(ActiveSweets* sweets)
    {
        m_cakeAnchorsDirty = true;
        if (!sweets)
        {
            return;
        }

        m_contactStates.erase(sweets);
    }

    void SweetsContacts::ClearSweetsContact(ActiveSweets* sweets)
    {
        m_cakeAnchorsDirty = true;
        if (!sweets)
        {
            return;
        }

        m_hasSweetsContact.erase(sweets);
    }

    void SweetsContacts::Reset()
    {
        m_cakeAnchorsDirty = true;
        m_cakeAnchoredSweets.clear();
        m_contactGraph.clear();
        m_contactQueue.clear();
        m_contactStates.clear();
        m_hasSweetsContact.clear();
        m_pendingGlue.clear();
        m_glueCooldown.clear();
    }

    void SweetsContacts::RemoveCreamWeldJointsForSweets(ActiveSweets* sweets, const char* context)
    {
        m_cakeAnchorsDirty = true;
        if (!sweets)
        {
            return;
        }
        RemovePendingGlueForSweets(sweets);
        if (!context || std::string(context) != "impactBreak")
        {
            m_glueCooldown.erase(sweets);
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
        }

    }

    void SweetsContacts::ClearCreamWeldJoints(const char* context)
    {
        m_cakeAnchorsDirty = true;
        m_pendingGlue.clear();
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
        m_glueCooldown.clear();
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

        // ミカ：ケーキへの到達性は接着候補ごとではなく今回の処理全体で一度だけ調べる
        const auto& cakeAnchoredSweets = GetCakeAnchoredSweets();
        for (auto it = m_pendingGlue.begin(); it != m_pendingGlue.end();)
        {
            const PendingGlue pending = it->second;
            Entities::PhysicalEntity* target = pending.fruit ? pending.fruit : pending.cake;
            if (!pending.cream || !target || pending.cream->IsPendingDestroy()
                || (pending.fruit && pending.fruit->IsPendingDestroy()))
            {
                it = m_pendingGlue.erase(it);
                continue;
            }

            ContactAnchorData anchorData{};
            if (!TryGetContactAnchorData(pending.contactId, pending.shapeIdA, pending.shapeIdB, anchorData))
            {
                it = m_pendingGlue.erase(it);
                continue;
            }

            // ミカ：接触が続く間は速度制限と再接着待ち時間の解除を待つ
            if ((pending.fruit && IsGlueCooldownActive(*pending.fruit))
                || IsGlueSpeedTooFast(*pending.cream) || IsGlueSpeedTooFast(*target))
            {
                ++it;
                continue;
            }

            if ((pending.fruit && !cakeAnchoredSweets.contains(pending.cream))
                || !HasSupportingContact(*pending.cream, nullptr)
                || (pending.fruit && !HasSupportingContact(*pending.fruit, nullptr)))
            {
                ++it;
                continue;
            }

            CleanupInvalidAnchor(pending.cream);
            auto existing = m_creamWeldJoints.find(pending.contactId);
            if (existing != m_creamWeldJoints.end())
            {
                CleanupInvalidJoint(existing->first, existing->second);
            }
            if (m_creamWeldJoints.contains(pending.contactId)
                || (pending.cake && m_creamAnchorJoints.contains(pending.cream)))
            {
                it = m_pendingGlue.erase(it);
                continue;
            }

            b2Vec2 anchor = anchorData.point;
            Entities::PhysicalEntity* movingBody = pending.fruit ? pending.fruit : pending.cream;
            const bool movingIsShapeA = DoesShapeBelongToEntity(*movingBody, anchorData.shapeIdA);
            const bool movingIsShapeB = DoesShapeBelongToEntity(*movingBody, anchorData.shapeIdB);
            if (movingIsShapeA != movingIsShapeB)
            {
                ApplyGluePushOut(*movingBody, movingIsShapeA, anchorData, anchor);
            }

            const b2JointId jointId = m_physicsMachine->CreateWeldJoint(
                *pending.cream,
                *target,
                anchor,
                m_glueTuning.linearHertz,
                m_glueTuning.linearDampingRatio,
                m_glueTuning.angularHertz,
                m_glueTuning.angularDampingRatio,
                m_glueTuning.collideConnected);
            if (b2Joint_IsValid(jointId))
            {
                if (pending.fruit)
                {
                    CreamWeldEntry entry{};
                    entry.cream = pending.cream;
                    entry.fruit = pending.fruit;
                    entry.jointId = jointId;
                    entry.anchorWorld = anchor;
                    m_creamWeldJoints.emplace(pending.contactId, entry);
                    ++m_fruitToCreams[pending.fruit][pending.cream];
                    LogGlueCreateFruitToCream(pending.cream, pending.fruit, anchor);
                }
                else
                {
                    m_creamAnchorJoints.emplace(pending.cream, jointId);
                    m_creamAnchorPoints.emplace(pending.cream, anchor);
                    LogGlueCreateCreamToCake(pending.cream, pending.cake, anchor);
                }
            }
            else
            {
                LogGlueReject("createFailed", pending.cream, target);
            }

            it = m_pendingGlue.erase(it);
        }
    }

    void SweetsContacts::OnStateChanged(ActiveSweets* sweets, SweetsState previousState, SweetsState newState)
    {
        m_cakeAnchorsDirty = true;
        (void)previousState;
        if (newState == SweetsState::Carried && sweets)
        {
            m_hasSweetsContact.erase(sweets);
        }
    }

    void SweetsContacts::TickDebugStats(float dt)
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

    void SweetsContacts::TickGlueCooldowns(float dt)
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

    bool SweetsContacts::IsGlueCooldownActive(const ActiveSweets& fruit) const
    {
        auto it = m_glueCooldown.find(const_cast<ActiveSweets*>(&fruit));
        if (it == m_glueCooldown.end())
        {
            return false;
        }

        return it->second > 0.0f;
    }

    SweetsContacts::GlueDebugSnapshot SweetsContacts::BuildDebugSnapshot() const
    {
        GlueDebugSnapshot snapshot{};
        snapshot.activeCreamAnchors = static_cast<int>(m_creamAnchorJoints.size());
        snapshot.activeFruitGlue = static_cast<int>(m_creamWeldJoints.size());
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
            if (b2Joint_IsValid(entry.second.jointId))
            {
                const b2BodyId bodyA = b2Joint_GetBodyA(entry.second.jointId);
                const b2Transform localFrameA = b2Joint_GetLocalFrameA(entry.second.jointId);
                link.anchorWorld = b2Body_GetWorldPoint(bodyA, localFrameA.p);
            }
            link.cream = entry.second.cream;
            link.fruit = entry.second.fruit;
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
