#include "Sweets/SweetsContacts.h"

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
            m_glueTuning.linearHertz = tuning.creamWeldLinearHertz;
            m_glueTuning.linearDampingRatio = tuning.creamWeldLinearDampingRatio;
            m_glueTuning.angularHertz = tuning.creamWeldAngularHertz;
            m_glueTuning.angularDampingRatio = tuning.creamWeldAngularDampingRatio;
            m_glueTuning.collideConnected = tuning.creamWeldCollideConnected;
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
                    b2Vec2 anchor{};
                    if (!TryGetContactAnchorWorld(info, anchor))
                    {
                        LogGlueReject("missingManifold", creamForStage, otherStage);
                    }
                    else
                    {
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
                if (!m_creamAnchorJoints.contains(cream))
                {
                    LogGlueReject("notAnchored", cream, otherSweets);
                }
                else if (IsCreamTag(otherSweets->GetTag()))
                {
                    LogGlueReject("targetIsCream", cream, otherSweets);
                }
                else if (IsBlockedTag(otherSweets->GetTag()))
                {
                    LogGlueReject("blockedTag", cream, otherSweets);
                }
                else if (m_physicsMachine)
                {
                    const auto fruitBinding = m_fruitToCream.find(otherSweets);
                    if (fruitBinding != m_fruitToCream.end())
                    {
                        LogGlueReject(fruitBinding->second == cream ? "alreadyBonded" : "alreadyGlued", cream, otherSweets);
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
                            b2Vec2 anchor{};
                            if (!TryGetContactAnchorWorld(info, anchor))
                            {
                                LogGlueReject("missingManifold", cream, otherSweets);
                            }
                            else
                            {
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
                                    m_fruitToCream.emplace(otherSweets, cream);
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

        if (!m_loggedContactNotification)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "'" << a->GetTag() << "' と '" << b->GetTag() << "' が接触し始めたよ \n";
            DebugTrace(oss.str());
            m_loggedContactNotification = true;
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

        if (!m_loggedContactNotification)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "'" << a->GetTag() << "' と '" << b->GetTag() << "' の接触が終わったよ \n";
            DebugTrace(oss.str());
            m_loggedContactNotification = true;
        }
    }

    bool SweetsContacts::TryGetContactAnchorWorld(const Systems::ContactEventInfo& info, b2Vec2& outAnchor) const
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

                outAnchor = data.manifold.points[0].point;
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

    void SweetsContacts::LogGlueReject(const char* reason, const ActiveSweets* cream, const Entities::PhysicalEntity* target) const
    {
        std::ostringstream oss;
        oss << kLogPrefix << "glue_reject_reason=" << (reason ? reason : "<unknown>")
            << " cream='" << (cream ? cream->GetTag() : "<null>")
            << "' target='" << (target ? target->GetTag() : "<null>") << "'\n";
        DebugTrace(oss.str());
    }

    void SweetsContacts::LogGlueDestroy(const char* reason, const ActiveSweets* cream, const Entities::PhysicalEntity* target) const
    {
        std::ostringstream oss;
        oss << kLogPrefix << "glue_destroy_reason=" << (reason ? reason : "<unknown>")
            << " cream='" << (cream ? cream->GetTag() : "<null>")
            << "' target='" << (target ? target->GetTag() : "<null>") << "'\n";
        DebugTrace(oss.str());
    }

    void SweetsContacts::LogGlueCreateCreamToCake(const ActiveSweets* cream, const Entities::PhysicalEntity* cake,
        const b2Vec2& anchorWorld) const
    {
        std::ostringstream oss;
        oss << kLogPrefix << "glue_create_cream_to_cake cream='" << (cream ? cream->GetTag() : "<null>")
            << "' cake='" << (cake ? cake->GetTag() : "<null>")
            << "' anchorWorld=(" << anchorWorld.x << "," << anchorWorld.y << ")\n";
        DebugTrace(oss.str());
    }

    void SweetsContacts::LogGlueCreateFruitToCream(const ActiveSweets* cream, const ActiveSweets* fruit,
        const b2Vec2& anchorWorld) const
    {
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
            m_fruitToCream.erase(fruit);
            m_creamWeldJoints.erase(key);
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
                m_fruitToCream.erase(fruit);
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
        m_fruitToCream.clear();

        for (const auto& entry : m_creamAnchorJoints)
        {
            LogGlueDestroy(reason, entry.first, nullptr);
            if (m_physicsMachine)
            {
                m_physicsMachine->DestroyJoint(entry.second);
            }
        }
        m_creamAnchorJoints.clear();
    }

    void SweetsContacts::OnStateChanged(ActiveSweets* sweets, SweetsState previousState, SweetsState newState)
    {
        (void)previousState;
        if (newState == SweetsState::Carried && sweets)
        {
            m_hasSweetsContact.erase(sweets);
        }
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
