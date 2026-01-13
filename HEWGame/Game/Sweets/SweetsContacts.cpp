#include "Sweets/SweetsContacts.h"

#include <algorithm>
#include <sstream>
#include <vector>

#include "CollisionLayer.h"
#include "MaterialDX.h"
#include "ModelDX.h"
#include "SceneGameConfigTypes.h"
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
    constexpr const char* kStageCakeKey = "cake";
    constexpr const char* kBlockedOsaraKey = "osara";
    constexpr const char* kBlockedFairyKey = "fairy";

    bool IsCreamTag(const std::string& tag)
    {
        static const std::string kCreamKey = SweetsSpace::NormalizeKey(::SweetsSpace::ToString(::SweetsSpace::SweetsType::Cream));
        return SweetsSpace::NormalizeKey(tag) == kCreamKey;
    }

    bool IsBlockedCreamTargetTag(const std::string& tag)
    {
        const std::string normalized = SweetsSpace::NormalizeKey(tag);
        return normalized == kBlockedOsaraKey || normalized == kBlockedFairyKey;
    }
}

namespace SweetsSpace
{
    SweetsContacts::SweetsContacts(Systems::PhysicsMachine* physicsMachine, bool* warnedMissingPhysicsMachine)
        : m_physicsMachine(physicsMachine)
        , m_warnedMissingPhysicsMachine(warnedMissingPhysicsMachine)
    {
    }

    void SweetsContacts::ConfigureCreamAdhesion(const SceneGameConfig::CreamAdhesionSettings& settings)
    {
        m_creamAdhesionDelaySeconds = std::max(0.0f, settings.adhesionDelaySeconds);
        m_creamAllowedTags.clear();
        for (const auto& tag : settings.allowedTags)
        {
            const std::string normalized = NormalizeKey(tag);
            if (!normalized.empty())
            {
                m_creamAllowedTags.insert(normalized);
            }
        }

        m_creamBlockedTags.clear();
        for (const auto& tag : settings.blockedTags)
        {
            const std::string normalized = NormalizeKey(tag);
            if (!normalized.empty())
            {
                m_creamBlockedTags.insert(normalized);
            }
        }
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
        Entities::PhysicalEntity* other = nullptr;
        if (sweetsA && IsCreamTag(sweetsA->GetTag()))
        {
            cream = sweetsA;
            other = b;
        }
        else if (sweetsB && IsCreamTag(sweetsB->GetTag()))
        {
            cream = sweetsB;
            other = a;
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
            else if (IsAdhesionTargetAllowed(*cream, *other))
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
                    auto pendingIt = m_pendingAdhesions.find(key);
                    if (pendingIt == m_pendingAdhesions.end())
                    {
                        PendingAdhesion pending{};
                        pending.cream = cream;
                        pending.other = other;
                        pending.elapsedSeconds = 0.0f;
                        pending.stillTouching = true;
                        m_pendingAdhesions.emplace(key, pending);
                    }
                    else
                    {
                        pendingIt->second.cream = cream;
                        pendingIt->second.other = other;
                        pendingIt->second.elapsedSeconds = 0.0f;
                        pendingIt->second.stillTouching = true;
                    }
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
        Entities::PhysicalEntity* other = nullptr;
        if (sweetsA && IsCreamTag(sweetsA->GetTag()))
        {
            cream = sweetsA;
            other = b;
        }
        else if (sweetsB && IsCreamTag(sweetsB->GetTag()))
        {
            cream = sweetsB;
            other = a;
        }

        if (cream && other)
        {
            const JointPairKey key = JointPairKey::Make(cream, other);
            m_pendingAdhesions.erase(key);
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

    void SweetsContacts::OnUpdate(float dt)
    {
        if (m_pendingAdhesions.empty())
        {
            return;
        }

        if (dt <= 0.0f)
        {
            return;
        }

        for (auto it = m_pendingAdhesions.begin(); it != m_pendingAdhesions.end();)
        {
            PendingAdhesion& pending = it->second;
            if (!pending.cream || !pending.other || !pending.stillTouching)
            {
                it = m_pendingAdhesions.erase(it);
                continue;
            }

            pending.elapsedSeconds += dt;
            if (pending.elapsedSeconds < m_creamAdhesionDelaySeconds)
            {
                ++it;
                continue;
            }

            if (!pending.cream->WasEverCarried() || !IsAdhesionTargetAllowed(*pending.cream, *pending.other))
            {
                it = m_pendingAdhesions.erase(it);
                continue;
            }

            TryCreateCreamWeldJoint(pending.cream, pending.other);
            it = m_pendingAdhesions.erase(it);
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

    void SweetsContacts::ClearPendingAdhesionsForSweets(ActiveSweets* sweets)
    {
        if (!sweets || m_pendingAdhesions.empty())
        {
            return;
        }

        for (auto it = m_pendingAdhesions.begin(); it != m_pendingAdhesions.end();)
        {
            if (it->second.cream == sweets || it->second.other == sweets)
            {
                it = m_pendingAdhesions.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void SweetsContacts::Reset()
    {
        m_contactStates.clear();
        m_hasSweetsContact.clear();
        m_pendingAdhesions.clear();
    }

    void SweetsContacts::RemoveCreamWeldJointsForSweets(ActiveSweets* sweets, const char* context)
    {
        if (!sweets)
        {
            return;
        }

        ClearPendingAdhesionsForSweets(sweets);
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
            m_pendingAdhesions.clear();
            return;
        }

        m_pendingAdhesions.clear();
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
        if (otherLayer == Collision::CollisionLayer::Floor || otherLayer == Collision::CollisionLayer::Stage)
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
        if ((otherLayer == Collision::CollisionLayer::Floor || otherLayer == Collision::CollisionLayer::Stage)
            && state.foundationContacts > 0)
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

    bool SweetsContacts::IsAdhesionTargetAllowed(const ActiveSweets& cream, const Entities::PhysicalEntity& other) const
    {
        if (&other == &cream)
        {
            return false;
        }

        if (IsBlockedCreamTargetTag(other.GetTag()))
        {
            return false;
        }

        if (IsCreamTag(other.GetTag()))
        {
            return false;
        }

        const bool isSweetsTarget = dynamic_cast<const ActiveSweets*>(&other) != nullptr;
        const Collision::CollisionLayer otherLayer = other.GetCollisionLayer();
        if (!isSweetsTarget && otherLayer != Collision::CollisionLayer::Stage)
        {
            return false;
        }

        std::vector<std::string> keys;
        const std::string tagKey = NormalizeKey(other.GetTag());
        if (otherLayer == Collision::CollisionLayer::Stage && tagKey != kStageCakeKey)
        {
            return false;
        }
        if (!tagKey.empty())
        {
            keys.push_back(tagKey);
        }

        if (const auto& colliderComponent = other.GetBoxColliderComponent())
        {
            const std::string colliderKey = NormalizeKey(colliderComponent->colliderId);
            if (!colliderKey.empty())
            {
                keys.push_back(colliderKey);
            }
        }

        if (const auto* model = other.GetRenderableModel())
        {
            if (auto material = model->GetMaterial(0).lock())
            {
                const std::string materialKey = NormalizeKey(material->GetMaterialKey());
                if (!materialKey.empty())
                {
                    keys.push_back(materialKey);
                }
            }
        }

        if (!m_creamBlockedTags.empty())
        {
            for (const auto& key : keys)
            {
                if (m_creamBlockedTags.find(key) != m_creamBlockedTags.end())
                {
                    return false;
                }
            }
        }

        if (!m_creamAllowedTags.empty())
        {
            bool allowed = false;
            for (const auto& key : keys)
            {
                if (m_creamAllowedTags.find(key) != m_creamAllowedTags.end())
                {
                    allowed = true;
                    break;
                }
            }

            if (!allowed)
            {
                return false;
            }
        }

        return true;
    }

    void SweetsContacts::TryCreateCreamWeldJoint(ActiveSweets* cream, Entities::PhysicalEntity* other)
    {
        if (!cream || !other)
        {
            return;
        }

        if (!m_physicsMachine)
        {
            if (m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
            {
                DebugTrace(std::string(kLogPrefix) + "クリーム接着を作れなかったよ 物理マシン未設定 \n");
                *m_warnedMissingPhysicsMachine = true;
            }
            return;
        }

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

        if (existing != m_creamWeldJoints.end())
        {
            return;
        }

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
        if (!b2Joint_IsValid(jointId))
        {
            return;
        }

        m_creamWeldJoints.emplace(key, jointId);

        if (cream->GetPhysical().IsDynamic())
        {
            cream->SetLinearVelocity(float3{ 0.0f, 0.0f, 0.0f });
            cream->SetAngularVelocity(0.0f);
        }

        if (other->GetPhysical().IsDynamic())
        {
            other->SetLinearVelocity(float3{ 0.0f, 0.0f, 0.0f });
            other->SetAngularVelocity(0.0f);
        }

        const bool creamSupported = HasSupportingContact(*cream, nullptr);
        bool otherSupported = true;
        if (auto* otherSweets = dynamic_cast<ActiveSweets*>(other))
        {
            otherSupported = HasSupportingContact(*otherSweets, nullptr);
        }

        const bool shouldSleep = creamSupported && otherSupported;
        if (cream->HasBox2DBody() && cream->GetPhysical().IsDynamic())
        {
            b2Body_SetAwake(cream->GetBox2DBodyId(), !shouldSleep);
        }

        if (other->HasBox2DBody() && other->GetPhysical().IsDynamic())
        {
            b2Body_SetAwake(other->GetBox2DBodyId(), !shouldSleep);
        }

        std::ostringstream cementLog;
        cementLog << kLogPrefix << "cementで '" << cream->GetTag() << "' と '" << other->GetTag()
                  << "' を溶接したよ \n";
        DebugTrace(cementLog.str());
    }
}
