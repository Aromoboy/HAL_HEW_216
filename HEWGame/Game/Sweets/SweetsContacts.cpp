#include "Sweets/SweetsContacts.h"

#include <sstream>

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
    constexpr float kCreamWeldLinearHertz = 6.0f;
    constexpr float kCreamWeldAngularHertz = 6.0f;
    constexpr float kCreamWeldLinearDampingRatio = 0.7f;
    constexpr float kCreamWeldAngularDampingRatio = 0.7f;

    bool IsCreamTag(const std::string& tag)
    {
        static const std::string kCreamKey = SweetsSpace::NormalizeKey(::SweetsSpace::ToString(::SweetsSpace::SweetsType::Cream));
        return SweetsSpace::NormalizeKey(tag) == kCreamKey;
    }
}

namespace SweetsSpace
{
    SweetsContacts::SweetsContacts(Systems::PhysicsMachine* physicsMachine, bool* warnedMissingPhysicsMachine)
        : m_physicsMachine(physicsMachine)
        , m_warnedMissingPhysicsMachine(warnedMissingPhysicsMachine)
    {
    }

    void SweetsContacts::OnContactBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b)
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
        ActiveSweets* other = nullptr;
        if (sweetsA && sweetsB)
        {
            if (IsCreamTag(sweetsA->GetTag()))
            {
                cream = sweetsA;
                other = sweetsB;
            }
            else if (IsCreamTag(sweetsB->GetTag()))
            {
                cream = sweetsB;
                other = sweetsA;
            }
        }

        if (cream && other)
        {
            if (!cream->WasEverCarried())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "クリーム '" << cream->GetTag()
                    << "' はまだ運ばれてないから接着をスキップするよ \n";
                DebugTrace(oss.str());
            }
            else if (m_physicsMachine)
            {
                const JointPairKey key = JointPairKey::Make(cream, other);
                auto existing = m_creamWeldJoints.find(key);
                if (existing != m_creamWeldJoints.end() && !b2Joint_IsValid(existing->second))
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "壊れたクリーム接着ジョイントを掃除したよ cream='"
                        << cream->GetTag() << "' other='" << other->GetTag() << "'\n";
                    DebugTrace(oss.str());
                    m_creamWeldJoints.erase(existing);
                    existing = m_creamWeldJoints.end();
                }

                if (existing == m_creamWeldJoints.end())
                {
                    const auto& posA = cream->GetTransform().position;
                    const auto& posB = other->GetTransform().position;
                    const b2Vec2 anchor{ 0.5f * (posA.x + posB.x), 0.5f * (posA.y + posB.y) };
                    const b2JointId jointId = m_physicsMachine->CreateWeldJoint(
                        *cream,
                        *other,
                        anchor,
                        kCreamWeldLinearHertz,
                        kCreamWeldLinearDampingRatio,
                        kCreamWeldAngularHertz,
                        kCreamWeldAngularDampingRatio,
                        false);
                    if (b2Joint_IsValid(jointId))
                    {
                        m_creamWeldJoints.emplace(key, jointId);
                        cream->SetLinearVelocity(float3{ 0.0f, 0.0f, 0.0f });
                        cream->SetAngularVelocity(0.0f);
                        other->SetLinearVelocity(float3{ 0.0f, 0.0f, 0.0f });
                        other->SetAngularVelocity(0.0f);
                        const bool creamSupported = HasSupportingContact(*cream, nullptr);
                        const bool otherSupported = HasSupportingContact(*other, nullptr);
                        const bool shouldSleep = creamSupported && otherSupported;
                        if (cream->HasBox2DBody())
                        {
                            b2Body_SetAwake(cream->GetBox2DBodyId(), !shouldSleep);
                        }

                        if (other->HasBox2DBody())
                        {
                            b2Body_SetAwake(other->GetBox2DBodyId(), !shouldSleep);
                        }

                        std::ostringstream cementLog;
                        cementLog << kLogPrefix << "cementで '" << cream->GetTag() << "' と '" << other->GetTag()
                                  << "' を溶接したよ \n";
                        DebugTrace(cementLog.str());
                    }
                }
            }
            else if (m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
            {
                DebugTrace(std::string(kLogPrefix) + "クリーム接着を作れなかったよ 物理マシン未設定 \n");
                *m_warnedMissingPhysicsMachine = true;
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

    void SweetsContacts::OnContactEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b)
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

        ActiveSweets* cream = nullptr;
        ActiveSweets* other = nullptr;
        if (sweetsA && sweetsB)
        {
            if (IsCreamTag(sweetsA->GetTag()))
            {
                cream = sweetsA;
                other = sweetsB;
            }
            else if (IsCreamTag(sweetsB->GetTag()))
            {
                cream = sweetsB;
                other = sweetsA;
            }
        }

        if (cream && other)
        {
            const JointPairKey key = JointPairKey::Make(cream, other);
            auto it = m_creamWeldJoints.find(key);
            if (it != m_creamWeldJoints.end())
            {
                if (m_physicsMachine && b2Joint_IsValid(it->second))
                {
                    m_physicsMachine->DestroyJoint(it->second);
                    std::ostringstream cementLog;
                    cementLog << kLogPrefix << "cementの溶接を外したよ '" << cream->GetTag() << "' と '"
                              << other->GetTag() << "'\n";
                    DebugTrace(cementLog.str());
                }
                m_creamWeldJoints.erase(it);
            }
        }

        if (!m_loggedContactNotification)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "'" << a->GetTag() << "' と '" << b->GetTag() << "' の接触が終わったよ \n";
            DebugTrace(oss.str());
            m_loggedContactNotification = true;
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
        if (!sweets || m_creamWeldJoints.empty())
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

        for (auto it = m_creamWeldJoints.begin(); it != m_creamWeldJoints.end();)
        {
            if (it->first.first == sweets || it->first.second == sweets)
            {
                if (m_physicsMachine)
                {
                    m_physicsMachine->DestroyJoint(it->second);
                }
                it = m_creamWeldJoints.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void SweetsContacts::ClearCreamWeldJoints(const char* context)
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

        for (const auto& entry : m_creamWeldJoints)
        {
            if (m_physicsMachine)
            {
                m_physicsMachine->DestroyJoint(entry.second);
            }
        }
        m_creamWeldJoints.clear();
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
