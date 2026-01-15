#include "Sweets/SweetsContacts.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string_view>
#include <vector>

#include "CollisionLayer.h"
#include "MaterialDX.h"
#include "ModelDX.h"
#include "Geometory.h"
#include "SceneGameConfigTypes.h"
#include "Sweets/BaseSweets.h"
#include "Sweets/SweetsRegistry.h"
#include "Sweets/SweetsKeyUtils.h"
#include "SweetsPrototypeManifest.h"
#include "Systems/Physics/PhysicsMachine.h"
#include "Diagnostics/Logger.h"

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

    void LogInfoAndTrace(const std::string& message)
    {
        Diagnostics::LogInfo(message);
        DebugTrace(message);
    }

    constexpr const char* kLogPrefix = "[SweetsManager] ";
    constexpr std::size_t kInvalidContactLogInterval = 50;
    constexpr std::size_t kCreamAdhesionEventLogLimit = 6;
    constexpr float kCreamDebugDefaultZ = 0.02f;
    const std::string kStageCakeKey = "cake";
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

    bool IsStageSupportingTag(const std::string& tag)
    {
        return SweetsSpace::NormalizeKey(tag) != kBlockedOsaraKey;
    }

    bool IsPendingDeletion(const Entities::PhysicalEntity* entity)
    {
        const auto* sweets = dynamic_cast<const SweetsSpace::BaseSweets*>(entity);
        return sweets && (sweets->IsPendingDestroy() || sweets->IsSafeToDelete());
    }

    bool HasValidBodyAndShapes(const Entities::PhysicalEntity* entity)
    {
        if (!entity)
        {
            return false;
        }

        const b2BodyId bodyId = entity->GetBox2DBodyId();
        if (!b2Body_IsValid(bodyId))
        {
            return false;
        }

        const int shapeCount = b2Body_GetShapeCount(bodyId);
        if (shapeCount <= 0)
        {
            return false;
        }

        std::vector<b2ShapeId> shapes(static_cast<size_t>(shapeCount));
        const int writtenShapes = b2Body_GetShapes(bodyId, shapes.data(), shapeCount);
        shapes.resize(static_cast<size_t>(std::max(0, writtenShapes)));

        if (shapes.empty())
        {
            return false;
        }

        for (const b2ShapeId shapeId : shapes)
        {
            if (!b2Shape_IsValid(shapeId))
            {
                return false;
            }
        }

        return true;
    }

    DirectX::XMFLOAT3 ToFloat3(const float3& value)
    {
        return DirectX::XMFLOAT3(value.x, value.y, value.z);
    }

    bool HasValidContactStateEntity(const Entities::PhysicalEntity* entity)
    {
        if (!entity)
        {
            return false;
        }

        if (!entity->HasBox2DBody())
        {
            return false;
        }

        const b2BodyId bodyId = entity->GetBox2DBodyId();
        if (!b2Body_IsValid(bodyId))
        {
            return false;
        }

        if (HasValidBodyAndShapes(entity))
        {
            return true;
        }

        return b2Body_GetShapeCount(bodyId) > 0;
    }

}

namespace SweetsSpace
{
    SweetsContacts::SweetsContacts(Systems::PhysicsMachine* physicsMachine,
        SweetsRegistry* registry,
        bool* warnedMissingPhysicsMachine)
        : m_physicsMachine(physicsMachine)
        , m_registry(registry)
        , m_warnedMissingPhysicsMachine(warnedMissingPhysicsMachine)
    {
    }

    void SweetsContacts::RegisterSweetsHandle(const std::shared_ptr<ActiveSweets>& sweets)
    {
        if (!sweets)
        {
            return;
        }

        m_sweetsHandleLookup[sweets.get()] = sweets;
    }

    void SweetsContacts::UnregisterSweetsHandle(ActiveSweets* sweets)
    {
        if (!sweets)
        {
            return;
        }

        m_sweetsHandleLookup.erase(sweets);
    }

    void SweetsContacts::RegisterAdhesionTarget(const std::shared_ptr<Entities::PhysicalEntity>& entity)
    {
        if (!entity)
        {
            return;
        }

        m_adhesionTargetLookup[entity.get()] = entity;
    }

    void SweetsContacts::UnregisterAdhesionTarget(Entities::PhysicalEntity* entity)
    {
        if (!entity)
        {
            return;
        }

        m_adhesionTargetLookup.erase(entity);
    }

    void SweetsContacts::ClearAdhesionTargets()
    {
        m_adhesionTargetLookup.clear();
    }

    void SweetsContacts::ConfigureCreamAdhesion(const SceneGameConfig::CreamAdhesionSettings& settings)
    {
        m_creamAdhesionDelaySeconds = std::max(0.0f, settings.adhesionDelaySeconds);
        m_creamAdhesionGraceSeconds = std::max(0.0f, settings.adhesionGraceSeconds);
        m_creamBreakImpulseThreshold = std::max(0.0f, settings.breakImpulseThreshold);
        m_creamReleaseCooldownSeconds = std::max(0.0f, settings.releaseCooldownSeconds);
        m_creamRequireSupport = settings.requireSupport;
        m_creamRequireCarried = settings.requireCarried;
        m_creamAllowAirAdhesionToSweets = settings.allowAirAdhesionToSweets;
        m_creamWeldLinearHertz = std::max(0.0f, settings.weldLinearHertz);
        m_creamWeldAngularHertz = std::max(0.0f, settings.weldAngularHertz);
        m_creamWeldLinearDampingRatio = std::max(0.0f, settings.weldLinearDampingRatio);
        m_creamWeldAngularDampingRatio = std::max(0.0f, settings.weldAngularDampingRatio);
        m_creamWeldBreakImpulseThreshold = std::max(0.0f, settings.breakImpulseThreshold);
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

        std::ostringstream oss;
        oss << kLogPrefix << "クリーム接着設定を適用したよ "
            << "delay=" << m_creamAdhesionDelaySeconds
            << " grace=" << m_creamAdhesionGraceSeconds
            << " requireSupport=" << std::boolalpha << m_creamRequireSupport
            << " requireCarried=" << std::boolalpha << m_creamRequireCarried
            << " allowAirAdhesionToSweets=" << std::boolalpha << m_creamAllowAirAdhesionToSweets
            << std::noboolalpha
            << " weldLinearHertz=" << m_creamWeldLinearHertz
            << " weldAngularHertz=" << m_creamWeldAngularHertz
            << " weldLinearDampingRatio=" << m_creamWeldLinearDampingRatio
            << " weldAngularDampingRatio=" << m_creamWeldAngularDampingRatio
            << " breakImpulseThreshold=" << m_creamWeldBreakImpulseThreshold
            << "\n";
        LogInfoAndTrace(oss.str());

        m_creamAdhesionConfig.adhesionDelaySeconds = m_creamAdhesionDelaySeconds;
        m_creamAdhesionConfig.adhesionGraceSeconds = m_creamAdhesionGraceSeconds;
        m_creamAdhesionConfig.releaseCooldownSeconds = m_creamReleaseCooldownSeconds;
        m_creamAdhesionConfig.requireSupport = settings.requireSupport;
        m_creamAdhesionConfig.requireCarried = settings.requireCarried;
        m_creamAdhesionConfig.weldLinearHertz = m_creamWeldLinearHertz;
        m_creamAdhesionConfig.weldAngularHertz = m_creamWeldAngularHertz;
        m_creamAdhesionConfig.weldLinearDampingRatio = m_creamWeldLinearDampingRatio;
        m_creamAdhesionConfig.weldAngularDampingRatio = m_creamWeldAngularDampingRatio;
        m_creamAdhesionConfig.breakImpulseThreshold = m_creamWeldBreakImpulseThreshold;
        m_creamAdhesionConfig.allowedTags = m_creamAllowedTags;
        m_creamAdhesionConfig.blockedTags = m_creamBlockedTags;
    }

    void SweetsContacts::SetResetting(bool resetting)
    {
        m_isResetting = resetting;
    }

    void SweetsContacts::OnContactBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b)
    {
        if (!HasValidBodyAndShapes(a) || !HasValidBodyAndShapes(b))
        {
            const std::size_t count = ++m_invalidContactEvents;
            if (count == 1 || (count % kInvalidContactLogInterval == 0))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "invalid contact filtered count=" << count
                    << " last=begin\n";
                LogInfoAndTrace(oss.str());
            }
            return;
        }

        if (m_isResetting)
        {
            return;
        }

        UpdateCreamAdhesionFromBegin(a, b);

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
            return;
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
        if (!HasValidBodyAndShapes(a) || !HasValidBodyAndShapes(b))
        {
            const std::size_t count = ++m_invalidContactEvents;
            if (count == 1 || (count % kInvalidContactLogInterval == 0))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "invalid contact filtered count=" << count
                    << " last=end\n";
                LogInfoAndTrace(oss.str());
            }
            return;
        }

        if (m_isResetting)
        {
            return;
        }

        UpdateCreamAdhesionFromEnd(a, b);

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
            return;
        }

        if (!m_loggedContactNotification)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "'" << a->GetTag() << "' と '" << b->GetTag() << "' の接触が終わったよ \n";
            DebugTrace(oss.str());
            m_loggedContactNotification = true;
        }
    }

    void SweetsContacts::OnContactLost(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b)
    {
        if (!a || !b)
        {
            return;
        }

        if (m_isResetting)
        {
            return;
        }

        UpdateCreamAdhesionFromLost(a, b);
    }

    void SweetsContacts::OnContactHit(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b, float approachSpeed)
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

        UpdateCreamAdhesionFromHit(a, b, approachSpeed);
    }

    void SweetsContacts::OnUpdate(float dt)
    {
        UpdateCreamAdhesionState(dt);
        CleanupInvalidContacts();
    }

    bool SweetsContacts::HasSupportingContact(const ActiveSweets& sweets, bool* foundationOnly) const
    {
        if (foundationOnly)
        {
            *foundationOnly = false;
        }

        const ContactState* state = FindContactState(sweets);
        if (!state)
        {
            return false;
        }

        const bool hasFoundation = state->foundationContacts > 0;
        const bool hasStage = state->stageContacts > 0;
        const bool hasSweetsContact = state->sweetsContacts > 0;
        if (foundationOnly)
        {
            *foundationOnly = (hasFoundation || hasStage) && !hasSweetsContact;
        }

        return hasFoundation || hasStage || hasSweetsContact;
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

    void SweetsContacts::ClearCreamAdhesionForEntity(Entities::PhysicalEntity* entity)
    {
        if (!entity)
        {
            return;
        }

        for (auto it = m_creamCandidates.begin(); it != m_creamCandidates.end();)
        {
            if (it->second.cream == entity || it->second.target == entity)
            {
                it = m_creamCandidates.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = m_creamBonds.begin(); it != m_creamBonds.end();)
        {
            if (it->second.cream == entity || it->second.target == entity)
            {
                if (m_physicsMachine && b2Joint_IsValid(it->second.jointId))
                {
                    m_physicsMachine->DestroyJoint(it->second.jointId);
                }
                const JointPairKey key = JointPairKey::Make(it->second.cream, it->second.target);
                if (auto jointIt = m_creamWeldJoints.find(key); jointIt != m_creamWeldJoints.end())
                {
                    m_creamWeldJoints.erase(jointIt);
                }
                it = m_creamBonds.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = m_creamWeldJoints.begin(); it != m_creamWeldJoints.end();)
        {
            if (it->first.first == entity || it->first.second == entity)
            {
                if (m_physicsMachine && b2Joint_IsValid(it->second))
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

    void SweetsContacts::RequestCreamWeldBreak(ActiveSweets* cream, Entities::PhysicalEntity* other, const char* context)
    {
        if (!cream || !other)
        {
            return;
        }

        BreakCreamWeldJoint(cream, other, context ? context : "RequestCreamWeldBreak", CreamBondBreakReason::Manual);
    }

    void SweetsContacts::Reset()
    {
        m_contactStates.clear();
        m_hasSweetsContact.clear();
        ClearCreamWeldJoints("Reset");
        m_sweetsHandleLookup.clear();
        m_loggedCreamPairEvents.clear();
        m_loggedContactNotification = false;
        m_creamCandidates.clear();
        m_creamBonds.clear();
        m_creamAdhesionClockSeconds = 0.0f;
        m_creamAdhesionEventLogCount = 0;
        m_creamRejectCounters = {};
        m_creamRejectLogTimestamp = -1.0f;
    }

    void SweetsContacts::RemoveCreamWeldJointsForSweets(ActiveSweets* sweets, const char* context)
    {
        if (!sweets)
        {
            return;
        }

        if (m_creamWeldJoints.empty() && m_creamBonds.empty())
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

        for (auto it = m_creamBonds.begin(); it != m_creamBonds.end();)
        {
            if (it->second.cream == sweets || it->second.target == sweets)
            {
                if (m_physicsMachine && b2Joint_IsValid(it->second.jointId))
                {
                    m_physicsMachine->DestroyJoint(it->second.jointId);
                }
                const JointPairKey key = JointPairKey::Make(it->second.cream, it->second.target);
                if (auto jointIt = m_creamWeldJoints.find(key); jointIt != m_creamWeldJoints.end())
                {
                    m_creamWeldJoints.erase(jointIt);
                }
                it = m_creamBonds.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void SweetsContacts::CleanupInvalidContacts()
    {
        std::vector<Entities::PhysicalEntity*> invalidEntities;

        const auto appendInvalidEntity = [&invalidEntities](Entities::PhysicalEntity* entity)
        {
            if (!entity)
            {
                return;
            }

            if (std::find(invalidEntities.begin(), invalidEntities.end(), entity) == invalidEntities.end())
            {
                invalidEntities.push_back(entity);
            }
        };

        for (auto it = m_contactStates.begin(); it != m_contactStates.end();)
        {
            if (!HasValidContactStateEntity(it->first))
            {
                appendInvalidEntity(it->first);
                m_hasSweetsContact.erase(it->first);
                it = m_contactStates.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (Entities::PhysicalEntity* entity : invalidEntities)
        {
            ClearCreamAdhesionForEntity(entity);

            if (auto* sweets = dynamic_cast<ActiveSweets*>(entity))
            {
                m_contactStates.erase(sweets);
                m_hasSweetsContact.erase(sweets);
            }

            std::ostringstream oss;
            oss << kLogPrefix << "invalid contact state purged\n";
            LogInfoAndTrace(oss.str());
        }
    };

    void SweetsContacts::ClearCreamWeldJoints(const char* context)
    {
        if (m_creamWeldJoints.empty() && m_creamBonds.empty())
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
        m_creamBonds.clear();
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
        else if (otherLayer == Collision::CollisionLayer::Stage && IsStageSupportingTag(other->GetTag()))
        {
            ++state.stageContacts;
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
        else if (otherLayer == Collision::CollisionLayer::Stage && IsStageSupportingTag(other->GetTag()))
        {
            if (state.stageContacts > 0)
            {
                --state.stageContacts;
            }
        }
        else if (otherLayer == Collision::CollisionLayer::Sweets && dynamic_cast<ActiveSweets*>(other))
        {
            if (state.sweetsContacts > 0)
            {
                --state.sweetsContacts;
            }
        }

        if (state.foundationContacts <= 0 && state.stageContacts <= 0 && state.sweetsContacts <= 0)
        {
            m_contactStates.erase(it);
        }
    }

    bool SweetsContacts::ResolveCreamAdhesionPair(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b,
        ActiveSweets*& cream, Entities::PhysicalEntity*& target) const
    {
        cream = nullptr;
        target = nullptr;

        if (!a || !b)
        {
            return false;
        }

        auto* sweetsA = dynamic_cast<ActiveSweets*>(a);
        auto* sweetsB = dynamic_cast<ActiveSweets*>(b);

        if (sweetsA && IsCreamTag(sweetsA->GetTag()))
        {
            cream = sweetsA;
            target = b;
        }
        else if (sweetsB && IsCreamTag(sweetsB->GetTag()))
        {
            cream = sweetsB;
            target = a;
        }

        return cream && target;
    }

    void SweetsContacts::LogCreamAdhesionEvent(const char* label, const ActiveSweets* cream,
        const Entities::PhysicalEntity* target)
    {
        if (!label || !cream || !target)
        {
            return;
        }

        if (m_creamAdhesionEventLogCount >= kCreamAdhesionEventLogLimit)
        {
            return;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "[CreamAdhesionV2] " << label
            << " cream='" << cream->GetTag()
            << "' target='" << target->GetTag() << "'\n";
        DebugTrace(oss.str());
        ++m_creamAdhesionEventLogCount;
    }

    void SweetsContacts::UpdateCreamAdhesionFromBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b)
    {
        ActiveSweets* cream = nullptr;
        Entities::PhysicalEntity* target = nullptr;
        if (!ResolveCreamAdhesionPair(a, b, cream, target))
        {
            return;
        }

        CreamAdhesionPairKey key{ cream, target };
        if (const auto bondIt = m_creamBonds.find(key);
            bondIt != m_creamBonds.end() && b2Joint_IsValid(bondIt->second.jointId))
        {
            return;
        }

        if (m_creamRequireCarried && !IsCreamCarried(*cream))
        {
            IncrementCreamAdhesionRejectCounter(CreamAdhesionRejectReason::RequireCarried);
            LogCreamAdhesionEvent("contact_begin_reject_not_carried", cream, target);
            if (ShouldLogCreamPairEvent(cream, target, "begin_reject_not_carried"))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "cream adhesion begin rejected (requireCarried="
                    << std::boolalpha << m_creamRequireCarried << std::noboolalpha
                    << ", isCarried=" << std::boolalpha << IsCreamCarried(*cream) << std::noboolalpha
                    << ") cream='" << cream->GetTag() << "' target='" << target->GetTag() << "'\n";
                LogInfoAndTrace(oss.str());
            }
            return;
        }

        auto [it, inserted] = m_creamCandidates.try_emplace(key);
        CreamAdhesionCandidate& candidate = it->second;
        candidate.cream = cream;
        candidate.target = target;
        candidate.touchingShapeCount = std::max(0, candidate.touchingShapeCount + 1);
        candidate.graceRemaining = std::max(candidate.graceRemaining, m_creamAdhesionConfig.adhesionGraceSeconds);
        if (inserted)
        {
            candidate.timeTouching = 0.0f;
            candidate.lastRejectReason = CreamAdhesionRejectReason::None;
        }

        LogCreamAdhesionEvent("contact_begin", cream, target);
        if (ShouldLogCreamPairEvent(cream, target, "begin_candidate"))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "cream adhesion begin candidate cream='" << cream->GetTag()
                << "' target='" << target->GetTag()
                << "' touchingShapes=" << candidate.touchingShapeCount
                << " grace=" << candidate.graceRemaining << "\n";
            LogInfoAndTrace(oss.str());
        }
    }

    void SweetsContacts::UpdateCreamAdhesionFromEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b)
    {
        ActiveSweets* cream = nullptr;
        Entities::PhysicalEntity* target = nullptr;
        if (!ResolveCreamAdhesionPair(a, b, cream, target))
        {
            return;
        }

        CreamAdhesionPairKey key{ cream, target };
        auto it = m_creamCandidates.find(key);
        if (it == m_creamCandidates.end())
        {
            return;
        }

        CreamAdhesionCandidate& candidate = it->second;
        candidate.touchingShapeCount = std::max(0, candidate.touchingShapeCount - 1);
        if (candidate.touchingShapeCount == 0)
        {
            candidate.graceRemaining = std::max(candidate.graceRemaining, m_creamAdhesionConfig.adhesionGraceSeconds);
        }

        LogCreamAdhesionEvent("contact_end", cream, target);
    }

    void SweetsContacts::UpdateCreamAdhesionFromLost(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b)
    {
        ActiveSweets* cream = nullptr;
        Entities::PhysicalEntity* target = nullptr;
        if (!ResolveCreamAdhesionPair(a, b, cream, target))
        {
            return;
        }

        CreamAdhesionPairKey key{ cream, target };
        if (auto it = m_creamCandidates.find(key); it != m_creamCandidates.end())
        {
            it->second.lastRejectReason = CreamAdhesionRejectReason::ContactLost;
            m_creamCandidates.erase(it);
        }

        if (auto it = m_creamBonds.find(key); it != m_creamBonds.end())
        {
            it->second.lastBreakReason = CreamBondBreakReason::ContactLost;
            BreakCreamWeldJoint(cream, target, "ContactLost", CreamBondBreakReason::ContactLost);
        }

        LogCreamAdhesionEvent("contact_lost", cream, target);
    }

    void SweetsContacts::UpdateCreamAdhesionFromHit(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b,
        float approachSpeed)
    {
        if (m_creamAdhesionConfig.breakImpulseThreshold <= 0.0f)
        {
            return;
        }

        ActiveSweets* cream = nullptr;
        Entities::PhysicalEntity* target = nullptr;
        if (!ResolveCreamAdhesionPair(a, b, cream, target))
        {
            return;
        }

        CreamAdhesionPairKey key{ cream, target };
        auto it = m_creamBonds.find(key);
        if (it == m_creamBonds.end() || !b2Joint_IsValid(it->second.jointId))
        {
            return;
        }

        float impulse = 0.0f;
        b2Vec2 contactPoint{};
        const bool hasContactPoint = TryGetContactPoint(*cream, *target, contactPoint, &impulse);
        (void)contactPoint;
        if (!hasContactPoint)
        {
            impulse = std::isfinite(approachSpeed) ? std::max(0.0f, approachSpeed) : 0.0f;
        }

        if (impulse < m_creamAdhesionConfig.breakImpulseThreshold)
        {
            return;
        }

        it->second.lastBreakReason = CreamBondBreakReason::ContactHit;
        it->second.lastBreakTimestamp = m_creamAdhesionClockSeconds;

        std::ostringstream oss;
        oss << kLogPrefix << "クリーム接着の破断閾値を超えたよ cream='"
            << cream->GetTag() << "' target='" << target->GetTag()
            << "' threshold=" << m_creamAdhesionConfig.breakImpulseThreshold
            << " impulse=" << impulse << "\n";
        LogInfoAndTrace(oss.str());

        BreakCreamWeldJoint(cream, target, "ContactHit", CreamBondBreakReason::ContactHit);
        LogCreamAdhesionEvent("contact_hit", cream, target);
    }

    void SweetsContacts::UpdateCreamAdhesionState(float dt)
    {
        if (dt <= 0.0f)
        {
            return;
        }

        m_creamAdhesionClockSeconds += dt;

        for (auto it = m_creamCandidates.begin(); it != m_creamCandidates.end();)
        {
            CreamAdhesionCandidate& candidate = it->second;
            if (!candidate.cream || !candidate.target
                || IsPendingDeletion(candidate.cream) || IsPendingDeletion(candidate.target)
                || !HasValidBodyAndShapes(candidate.cream) || !HasValidBodyAndShapes(candidate.target))
            {
                candidate.lastRejectReason = CreamAdhesionRejectReason::InvalidEntity;
                it = m_creamCandidates.erase(it);
                continue;
            }

            if (candidate.touchingShapeCount > 0)
            {
                candidate.timeTouching += dt;
            }
            else if (candidate.graceRemaining > 0.0f)
            {
                candidate.graceRemaining = std::max(0.0f, candidate.graceRemaining - dt);
                if (candidate.graceRemaining > 0.0f)
                {
                    ++it;
                    continue;
                }
            }
            else
            {
                candidate.lastRejectReason = CreamAdhesionRejectReason::ContactLost;
                it = m_creamCandidates.erase(it);
                continue;
            }

            if (candidate.touchingShapeCount <= 0 || candidate.timeTouching < m_creamAdhesionConfig.adhesionDelaySeconds)
            {
                ++it;
                continue;
            }

            if (ShouldLogCreamPairEvent(candidate.cream, candidate.target, "candidate_ready"))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "cream adhesion candidate ready cream='" << candidate.cream->GetTag()
                    << "' target='" << candidate.target->GetTag()
                    << "' touchingSeconds=" << candidate.timeTouching
                    << " touchingShapes=" << candidate.touchingShapeCount
                    << " grace=" << candidate.graceRemaining << "\n";
                LogInfoAndTrace(oss.str());
            }

            if (m_creamRequireCarried && !IsCreamCarried(*candidate.cream))
            {
                candidate.lastRejectReason = CreamAdhesionRejectReason::RequireCarried;
                IncrementCreamAdhesionRejectCounter(candidate.lastRejectReason);
                if (ShouldLogCreamPairEvent(candidate.cream, candidate.target, "candidate_reject_not_carried"))
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "cream adhesion candidate rejected (requireCarried="
                        << std::boolalpha << m_creamRequireCarried << std::noboolalpha
                        << ", isCarried=" << std::boolalpha << IsCreamCarried(*candidate.cream) << std::noboolalpha
                        << ") cream='" << candidate.cream->GetTag()
                        << "' target='" << candidate.target->GetTag() << "'\n";
                    LogInfoAndTrace(oss.str());
                }
                it = m_creamCandidates.erase(it);
                continue;
            }

            if (!IsAdhesionTargetAllowed(*candidate.cream, *candidate.target, &candidate.lastRejectReason))
            {
                IncrementCreamAdhesionRejectCounter(candidate.lastRejectReason);
                it = m_creamCandidates.erase(it);
                continue;
            }

            CreamAdhesionPairKey key{ candidate.cream, candidate.target };
            auto bondIt = m_creamBonds.find(key);
            if (bondIt != m_creamBonds.end() && b2Joint_IsValid(bondIt->second.jointId))
            {
                candidate.lastRejectReason = CreamAdhesionRejectReason::ExistingBond;
                IncrementCreamAdhesionRejectCounter(candidate.lastRejectReason);
                it = m_creamCandidates.erase(it);
                continue;
            }

            if (bondIt != m_creamBonds.end() && bondIt->second.cooldownRemaining > 0.0f)
            {
                candidate.lastRejectReason = CreamAdhesionRejectReason::CooldownActive;
                IncrementCreamAdhesionRejectCounter(candidate.lastRejectReason);
                ++it;
                continue;
            }

            if (!HasValidBodyAndShapes(candidate.cream) || !HasValidBodyAndShapes(candidate.target))
            {
                candidate.lastRejectReason = CreamAdhesionRejectReason::InvalidEntity;
                it = m_creamCandidates.erase(it);
                continue;
            }

            if (!m_physicsMachine)
            {
                candidate.lastRejectReason = CreamAdhesionRejectReason::MissingPhysics;
                it = m_creamCandidates.erase(it);
                continue;
            }

            if (!TryCreateCreamWeldJoint(candidate.cream, candidate.target, candidate.timeTouching))
            {
                candidate.lastRejectReason = CreamAdhesionRejectReason::JointCreateFailed;
                it = m_creamCandidates.erase(it);
                continue;
            }

            it = m_creamCandidates.erase(it);
        }

        for (auto it = m_creamBonds.begin(); it != m_creamBonds.end();)
        {
            CreamAdhesionBond& bond = it->second;
            if (!bond.cream || !bond.target || IsPendingDeletion(bond.cream) || IsPendingDeletion(bond.target)
                || !HasValidBodyAndShapes(bond.cream) || !HasValidBodyAndShapes(bond.target))
            {
                if (b2Joint_IsValid(bond.jointId) && m_physicsMachine)
                {
                    m_physicsMachine->DestroyJoint(bond.jointId);
                }
                it = m_creamBonds.erase(it);
                continue;
            }

            if (bond.cooldownRemaining > 0.0f)
            {
                bond.cooldownRemaining = std::max(0.0f, bond.cooldownRemaining - dt);
            }

            if (!b2Joint_IsValid(bond.jointId) && bond.cooldownRemaining <= 0.0f)
            {
                it = m_creamBonds.erase(it);
                continue;
            }

            ++it;
        }
    }

    bool SweetsContacts::IsAdhesionTargetAllowed(const ActiveSweets& cream, const Entities::PhysicalEntity& other,
        CreamAdhesionRejectReason* outReason) const
    {
        if (&other == &cream)
        {
            return false;
        }

        auto logRejectedTarget = [&](const char* reason)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "クリーム接着対象外 (reasonKey=" << reason << ") other='" << other.GetTag() << "'\n";
            LogInfoAndTrace(oss.str());
        };

        if (IsBlockedCreamTargetTag(other.GetTag()))
        {
            if (outReason)
            {
                *outReason = CreamAdhesionRejectReason::TargetBlocked;
            }
            logRejectedTarget("blocked tag");
            return false;
        }

        if (IsCreamTag(other.GetTag()))
        {
            if (outReason)
            {
                *outReason = CreamAdhesionRejectReason::TargetNotSupportedType;
            }
            logRejectedTarget("cream target");
            return false;
        }

        const bool isSweetsTarget = dynamic_cast<const ActiveSweets*>(&other) != nullptr;
        const Collision::CollisionLayer otherLayer = other.GetCollisionLayer();
        if (!isSweetsTarget && otherLayer != Collision::CollisionLayer::Stage)
        {
            if (outReason)
            {
                *outReason = CreamAdhesionRejectReason::TargetNotSupportedType;
            }
            logRejectedTarget("non-sweets");
            return false;
        }

        std::vector<std::string> keys;
        const std::string tagKey = NormalizeKey(other.GetTag());
        if (otherLayer == Collision::CollisionLayer::Stage && tagKey != kStageCakeKey)
        {
            if (outReason)
            {
                *outReason = CreamAdhesionRejectReason::TargetNotSupportedType;
            }
            logRejectedTarget("stage not cake");
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
                    if (outReason)
                    {
                        *outReason = CreamAdhesionRejectReason::TargetBlocked;
                    }
                    logRejectedTarget("blocked tag");
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
                if (outReason)
                {
                    *outReason = CreamAdhesionRejectReason::TargetNotAllowed;
                }
                logRejectedTarget("not in allowed list");
                return false;
            }
        }

        if (outReason)
        {
            *outReason = CreamAdhesionRejectReason::None;
        }
        return true;
    }

    bool SweetsContacts::TryCreateCreamWeldJoint(ActiveSweets* cream, Entities::PhysicalEntity* other, float touchingSeconds)
    {
        if (!cream || !other)
        {
            return false;
        }

        if (ShouldLogCreamPairEvent(cream, other, "weld_attempt"))
        {
            const auto shapeInfo = [](const Entities::PhysicalEntity* entity)
            {
                if (!entity)
                {
                    return std::string("entity=<null>");
                }

                const b2BodyId bodyId = entity->GetBox2DBodyId();
                if (!b2Body_IsValid(bodyId))
                {
                    return "body=invalid";
                }

                const int shapeCount = b2Body_GetShapeCount(bodyId);
                std::ostringstream oss;
                oss << "body=valid shapes=" << shapeCount;
                return oss.str();
            };

            std::ostringstream oss;
            oss << kLogPrefix << "cream weld attempt cream='" << cream->GetTag()
                << "' target='" << other->GetTag()
                << "' touchingSeconds=" << touchingSeconds
                << " [" << shapeInfo(cream) << "]"
                << " [" << shapeInfo(other) << "]\n";
            LogInfoAndTrace(oss.str());
        }

        auto contactCounts = [&](const ActiveSweets* target)
        {
            int foundationContacts = 0;
            int stageContacts = 0;
            int sweetsContacts = 0;
            if (target)
            {
                if (const ContactState* state = FindContactState(*target))
                {
                    foundationContacts = state->foundationContacts;
                    stageContacts = state->stageContacts;
                    sweetsContacts = state->sweetsContacts;
                }
            }
            std::ostringstream oss;
            oss << "foundationContacts=" << foundationContacts
                << " stageContacts=" << stageContacts
                << " sweetsContacts=" << sweetsContacts;
            if (!target)
            {
                oss << " (non-sweets)";
            }
            return oss.str();
        };

        auto logRefusedAdhesion = [&](const char* reason)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "クリーム接着を拒否 (" << reason << ") cream='" << cream->GetTag()
                << "' other='" << other->GetTag() << "' "
                << "[cream " << contactCounts(cream) << "] "
                << "[other " << contactCounts(dynamic_cast<ActiveSweets*>(other)) << "] "
                << "requireSupport=" << std::boolalpha << m_creamRequireSupport
                << " allowAirAdhesionToSweets=" << std::boolalpha << m_creamAllowAirAdhesionToSweets
                << std::noboolalpha << "\n";
            LogInfoAndTrace(oss.str());
        };

        if (!m_physicsMachine)
        {
            if (m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
            {
                LogInfoAndTrace(std::string(kLogPrefix) + "クリーム接着を作れなかったよ 物理マシン未設定 \n");
                *m_warnedMissingPhysicsMachine = true;
            }
            if (ShouldLogCreamPairEvent(cream, other, "missing_physics"))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "cream weld early return reason=missing physics cream='"
                    << cream->GetTag() << "' other='" << other->GetTag() << "'\n";
                LogInfoAndTrace(oss.str());
            }
            logRefusedAdhesion("物理マシン未設定");
            return false;
        }

        CreamAdhesionRejectReason targetReason = CreamAdhesionRejectReason::None;
        if (!IsAdhesionTargetAllowed(*cream, *other, &targetReason))
        {
            IncrementCreamAdhesionRejectCounter(targetReason);
            if (ShouldLogCreamPairEvent(cream, other, "target_disallowed"))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "cream weld early return reason=target disallowed cream='"
                    << cream->GetTag() << "' other='" << other->GetTag() << "'\n";
                LogInfoAndTrace(oss.str());
            }
            return false;
        }

        const bool creamLocked = IsBondedOrLocked(*cream);
        const bool otherLocked = IsBondedOrLocked(*other);
        const bool creamSupported = HasSupportingContact(*cream, nullptr);
        bool otherSupportedForAdhesion = false;
        bool otherSupportedForSleep = true;
        const JointPairKey key = JointPairKey::Make(cream, other);
        const bool pendingLongEnough = touchingSeconds >= m_creamAdhesionDelaySeconds;
        auto* otherSweets = dynamic_cast<ActiveSweets*>(other);
        const bool otherIsSweetsTarget = otherSweets != nullptr;
        const bool allowAirAdhesion = m_creamAllowAirAdhesionToSweets && otherIsSweetsTarget && pendingLongEnough;
        if (otherSweets)
        {
            otherSupportedForSleep = HasSupportingContact(*otherSweets, nullptr);
            otherSupportedForAdhesion = other->GetPhysical().IsDynamic() && otherSupportedForSleep;
        }

        if (m_creamRequireSupport && !creamSupported && !otherSupportedForAdhesion && !allowAirAdhesion
            && !(creamLocked && otherLocked))
        {
            if (ShouldLogCreamPairEvent(cream, other, "no_support"))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "cream weld early return reason=no support cream='"
                    << cream->GetTag() << "' other='" << other->GetTag() << "'\n";
                LogInfoAndTrace(oss.str());
            }
            logRefusedAdhesion("空中で支えがない");
            return false;
        }

        const ContactState* creamContact = FindContactState(*cream);
        const bool creamHasFoundationOrStage = creamContact
            && (creamContact->foundationContacts > 0 || creamContact->stageContacts > 0);
        if (m_creamRequireSupport && !creamHasFoundationOrStage && !creamLocked && !otherLocked
            && !otherSupportedForAdhesion && !allowAirAdhesion)
        {
            if (ShouldLogCreamPairEvent(cream, other, "no_foundation"))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "cream weld early return reason=no foundation cream='"
                    << cream->GetTag() << "' other='" << other->GetTag() << "'\n";
                LogInfoAndTrace(oss.str());
            }
            logRefusedAdhesion("接地がない");
            return false;
        }
        if (auto existingBond = m_creamBonds.find({ cream, other });
            existingBond != m_creamBonds.end() && b2Joint_IsValid(existingBond->second.jointId))
        {
            IncrementCreamAdhesionRejectCounter(CreamAdhesionRejectReason::ExistingBond);
            if (ShouldLogCreamPairEvent(cream, other, "existing_joint"))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "cream weld early return reason=existing joint cream='"
                    << cream->GetTag() << "' other='" << other->GetTag() << "'\n";
                LogInfoAndTrace(oss.str());
            }
            return false;
        }

        b2Vec2 anchor{};
        const bool hasAnchor = TryGetContactPoint(*cream, *other, anchor, nullptr);
        if (!hasAnchor)
        {
            const auto& posA = cream->GetTransform().position;
            const auto& posB = other->GetTransform().position;
            anchor = b2Vec2{ 0.5f * (posA.x + posB.x), 0.5f * (posA.y + posB.y) };
            if (ShouldLogCreamPairEvent(cream, other, "anchor_fallback"))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "cream weld anchor fallback (midpoint) cream='"
                    << cream->GetTag() << "' other='" << other->GetTag() << "'\n";
                LogInfoAndTrace(oss.str());
            }
        }
        const b2JointId jointId = m_physicsMachine->CreateWeldJoint(
            *cream,
            *other,
            anchor,
            m_creamWeldLinearHertz,
            m_creamWeldLinearDampingRatio,
            m_creamWeldAngularHertz,
            m_creamWeldAngularDampingRatio,
            false);
        if (!b2Joint_IsValid(jointId))
        {
            if (ShouldLogCreamPairEvent(cream, other, "joint_create_failed"))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "cream weld early return reason=joint create failed cream='"
                    << cream->GetTag() << "' other='" << other->GetTag() << "'\n";
                LogInfoAndTrace(oss.str());
            }
            std::ostringstream oss;
            oss << kLogPrefix << "クリーム接着ジョイント作成に失敗したよ cream='"
                << cream->GetTag() << "' other='" << other->GetTag() << "'\n";
            LogInfoAndTrace(oss.str());
            return false;
        }

        m_creamWeldJoints.emplace(key, jointId);
        CreamAdhesionPairKey bondKey{ cream, other };
        CreamAdhesionBond& bond = m_creamBonds[bondKey];
        bond.cream = cream;
        bond.target = other;
        bond.jointId = jointId;
        bond.createdAtSeconds = m_creamAdhesionClockSeconds;
        bond.cooldownRemaining = 0.0f;
        bond.lastBreakReason = CreamBondBreakReason::None;
        bond.lastBreakTimestamp = 0.0f;

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

        const bool shouldSleep = creamSupported && otherSupportedForSleep;
        if (cream->HasBox2DBody() && cream->GetPhysical().IsDynamic())
        {
            b2Body_SetAwake(cream->GetBox2DBodyId(), !shouldSleep);
        }

        if (other->HasBox2DBody() && other->GetPhysical().IsDynamic())
        {
            b2Body_SetAwake(other->GetBox2DBodyId(), !shouldSleep);
        }

        std::ostringstream bondLog;
        bondLog << kLogPrefix << "クリーム接着で '" << cream->GetTag() << "' と '" << other->GetTag()
                << "' を溶接したよ \n";
        LogInfoAndTrace(bondLog.str());
        return true;
    }

    void SweetsContacts::BreakCreamWeldJoint(ActiveSweets* cream, Entities::PhysicalEntity* other, const char* context,
        CreamBondBreakReason reason)
    {
        if (!cream || !other)
        {
            return;
        }

        const JointPairKey key = JointPairKey::Make(cream, other);
        auto jointIt = m_creamWeldJoints.find(key);
        auto bondIt = m_creamBonds.find({ cream, other });
        b2JointId jointId = b2_nullJointId;
        if (bondIt != m_creamBonds.end())
        {
            jointId = bondIt->second.jointId;
        }
        if (jointIt != m_creamWeldJoints.end())
        {
            jointId = jointIt->second;
        }
        if (!b2Joint_IsValid(jointId) && jointIt == m_creamWeldJoints.end())
        {
            return;
        }
        if (!m_physicsMachine)
        {
            if (m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
            {
                std::ostringstream oss;
                oss << kLogPrefix << (context ? context : "BreakCreamWeldJoint")
                    << " でクリーム接着ジョイントを外せなかったよ 物理マシン未設定 \n";
                DebugTrace(oss.str());
                *m_warnedMissingPhysicsMachine = true;
            }
        }
        else if (b2Joint_IsValid(jointId))
        {
            m_physicsMachine->DestroyJoint(jointId);
            std::ostringstream bondLog;
            bondLog << kLogPrefix << "クリーム接着の溶接を外したよ '" << cream->GetTag() << "' と '"
                    << other->GetTag() << "'\n";
            DebugTrace(bondLog.str());
        }

        if (jointIt != m_creamWeldJoints.end())
        {
            m_creamWeldJoints.erase(jointIt);
        }

        if (bondIt != m_creamBonds.end())
        {
            bondIt->second.jointId = b2_nullJointId;
            bondIt->second.lastBreakReason = reason;
            bondIt->second.lastBreakTimestamp = m_creamAdhesionClockSeconds;
            bondIt->second.cooldownRemaining = std::max(bondIt->second.cooldownRemaining,
                m_creamAdhesionConfig.releaseCooldownSeconds);
        }
        else
        {
            CreamAdhesionBond bond{};
            bond.cream = cream;
            bond.target = other;
            bond.jointId = b2_nullJointId;
            bond.lastBreakReason = reason;
            bond.lastBreakTimestamp = m_creamAdhesionClockSeconds;
            bond.cooldownRemaining = m_creamAdhesionConfig.releaseCooldownSeconds;
            m_creamBonds.emplace(CreamAdhesionPairKey{ cream, other }, bond);
        }
    }

    const SweetsContacts::ContactState* SweetsContacts::FindContactState(const ActiveSweets& sweets) const
    {
        auto it = m_contactStates.find(const_cast<ActiveSweets*>(&sweets));
        if (it == m_contactStates.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    bool SweetsContacts::ShouldLogCreamPairEvent(ActiveSweets* cream,
                                                 Entities::PhysicalEntity* other,
                                                 std::string_view eventKey)
    {
        if (!cream || !other || eventKey.empty())
        {
            return false;
        }

        const JointPairKey key = JointPairKey::Make(cream, other);
        const std::size_t pairHash = JointPairHash{}(key);
        const std::size_t eventHash = std::hash<std::string_view>{}(eventKey);
        const std::size_t combined = pairHash ^ (eventHash + 0x9e3779b9 + (pairHash << 6) + (pairHash >> 2));
        return m_loggedCreamPairEvents.insert(combined).second;
    }

    bool SweetsContacts::IsCakeEntity(const Entities::PhysicalEntity& entity) const
    {
        if (entity.GetCollisionLayer() != Collision::CollisionLayer::Stage)
        {
            return false;
        }

        return NormalizeKey(entity.GetTag()) == kStageCakeKey;
    }

    bool SweetsContacts::IsBondedOrLocked(const Entities::PhysicalEntity& entity) const
    {
        if (!entity.GetPhysical().IsDynamic())
        {
            return true;
        }

        for (const auto& entry : m_creamWeldJoints)
        {
            if (entry.first.first == &entity || entry.first.second == &entity)
            {
                return true;
            }
        }

        return false;
    }

    bool SweetsContacts::IsCreamCarried(const ActiveSweets& cream) const
    {
        if (!m_registry)
        {
            return false;
        }

        const auto handle = FindSweetsHandle(const_cast<ActiveSweets*>(&cream));
        if (handle.expired())
        {
            return false;
        }

        if (auto locked = handle.lock())
        {
            if (auto state = m_registry->GetState(locked))
            {
                return *state == SweetsState::Carried;
            }
        }

        return false;
    }

    void SweetsContacts::IncrementCreamAdhesionRejectCounter(CreamAdhesionRejectReason reason)
    {
        switch (reason)
        {
        case CreamAdhesionRejectReason::TargetBlocked:
            ++m_creamRejectCounters.blockedTag;
            break;
        case CreamAdhesionRejectReason::RequireCarried:
            ++m_creamRejectCounters.notCarried;
            break;
        case CreamAdhesionRejectReason::TargetNotAllowed:
            ++m_creamRejectCounters.notAllowed;
            break;
        case CreamAdhesionRejectReason::ExistingBond:
            ++m_creamRejectCounters.alreadyBonded;
            break;
        case CreamAdhesionRejectReason::CooldownActive:
            ++m_creamRejectCounters.cooldown;
            break;
        case CreamAdhesionRejectReason::TargetNotSupportedType:
            ++m_creamRejectCounters.targetNotSupportedType;
            break;
        default:
            break;
        }
    }

    void SweetsContacts::DebugLogCreamAdhesionRejectCounters(float minIntervalSeconds)
    {
        if (minIntervalSeconds <= 0.0f)
        {
            minIntervalSeconds = 0.0f;
        }

        if (m_creamRejectCounters.Total() == 0)
        {
            return;
        }

        if (m_creamRejectLogTimestamp >= 0.0f
            && (m_creamAdhesionClockSeconds - m_creamRejectLogTimestamp) < minIntervalSeconds)
        {
            return;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "[CreamAdhesionRejects] blockedTag=" << m_creamRejectCounters.blockedTag
            << " notCarried=" << m_creamRejectCounters.notCarried
            << " notAllowed=" << m_creamRejectCounters.notAllowed
            << " alreadyBonded=" << m_creamRejectCounters.alreadyBonded
            << " cooldown=" << m_creamRejectCounters.cooldown
            << " targetNotSupportedType=" << m_creamRejectCounters.targetNotSupportedType
            << " total=" << m_creamRejectCounters.Total()
            << "\n";
        LogInfoAndTrace(oss.str());
        m_creamRejectLogTimestamp = m_creamAdhesionClockSeconds;
    }

    void SweetsContacts::DebugDrawCreamAdhesion() const
    {
#if defined(_DEBUG)
        const DirectX::XMFLOAT4 activeColor(0.3f, 1.0f, 0.4f, 0.9f);
        const DirectX::XMFLOAT4 candidateColor(0.4f, 0.6f, 1.0f, 0.9f);
        const DirectX::XMFLOAT4 graceColor(1.0f, 0.85f, 0.2f, 0.9f);
        const DirectX::XMFLOAT4 cooldownColor(1.0f, 0.3f, 0.3f, 0.85f);

        const auto getPosition = [](const Entities::PhysicalEntity& entity)
        {
            return entity.GetTransform().position;
        };

        for (const auto& entry : m_creamBonds)
        {
            const CreamAdhesionBond& bond = entry.second;
            if (!bond.cream || !bond.target)
            {
                continue;
            }

            const float3 start = getPosition(*bond.cream);
            const float3 end = getPosition(*bond.target);
            if (bond.cooldownRemaining > 0.0f || !b2Joint_IsValid(bond.jointId))
            {
                Geometory::AddLine(ToFloat3(start), ToFloat3(end), cooldownColor);
            }
            else
            {
                Geometory::AddLine(ToFloat3(start), ToFloat3(end), activeColor);
            }
        }

        for (const auto& entry : m_creamCandidates)
        {
            const CreamAdhesionCandidate& candidate = entry.second;
            if (!candidate.cream || !candidate.target)
            {
                continue;
            }

            const float3 start = getPosition(*candidate.cream);
            const float3 end = getPosition(*candidate.target);
            if (candidate.graceRemaining > 0.0f && candidate.touchingShapeCount == 0)
            {
                Geometory::AddLine(ToFloat3(start), ToFloat3(end), graceColor);
            }
            else
            {
                Geometory::AddLine(ToFloat3(start), ToFloat3(end), candidateColor);
            }
        }

        for (const auto& entry : m_creamBonds)
        {
            const CreamAdhesionBond& bond = entry.second;
            if (!bond.cream || !bond.target)
            {
                continue;
            }

            if (bond.cooldownRemaining <= 0.0f)
            {
                continue;
            }

            const float3 base = getPosition(*bond.cream);
            const float barWidth = 0.3f;
            const float barHeight = 0.06f;
            const float maxCooldown = std::max(0.01f, m_creamAdhesionConfig.releaseCooldownSeconds);
            const float ratio = std::clamp(bond.cooldownRemaining / maxCooldown, 0.0f, 1.0f);
            const float filledWidth = barWidth * ratio;
            const float z = base.z + kCreamDebugDefaultZ;

            DirectX::XMFLOAT3 leftTop(base.x - barWidth * 0.5f, base.y + 0.4f, z);
            DirectX::XMFLOAT3 rightTop(base.x + barWidth * 0.5f, base.y + 0.4f, z);
            DirectX::XMFLOAT3 leftBottom(base.x - barWidth * 0.5f, base.y + 0.4f - barHeight, z);
            DirectX::XMFLOAT3 rightBottom(base.x + barWidth * 0.5f, base.y + 0.4f - barHeight, z);

            Geometory::AddLine(leftTop, rightTop, cooldownColor);
            Geometory::AddLine(leftBottom, rightBottom, cooldownColor);
            Geometory::AddLine(leftTop, leftBottom, cooldownColor);
            Geometory::AddLine(rightTop, rightBottom, cooldownColor);

            DirectX::XMFLOAT3 fillRight(base.x - barWidth * 0.5f + filledWidth, base.y + 0.4f, z);
            DirectX::XMFLOAT3 fillRightBottom(base.x - barWidth * 0.5f + filledWidth, base.y + 0.4f - barHeight, z);
            Geometory::AddLine(leftTop, fillRight, cooldownColor);
            Geometory::AddLine(leftBottom, fillRightBottom, cooldownColor);
            Geometory::AddLine(fillRight, fillRightBottom, cooldownColor);
        }
#endif
    }

    bool SweetsContacts::TryGetContactPoint(const Entities::PhysicalEntity& a, const Entities::PhysicalEntity& b,
        b2Vec2& outPoint, float* outImpulse) const
    {
        if (!a.HasBox2DBody() || !b.HasBox2DBody())
        {
            return false;
        }

        const b2BodyId bodyA = a.GetBox2DBodyId();
        const b2BodyId bodyB = b.GetBox2DBodyId();
        if (!b2Body_IsValid(bodyA) || !b2Body_IsValid(bodyB))
        {
            return false;
        }

        const int capacity = b2Body_GetContactCapacity(bodyA);
        if (capacity <= 0)
        {
            return false;
        }

        std::vector<b2ContactData> contacts(static_cast<std::size_t>(capacity));
        const int count = b2Body_GetContactData(bodyA, contacts.data(), capacity);
        if (count <= 0)
        {
            return false;
        }

        const uint64_t keyA = b2StoreBodyId(bodyA);
        const uint64_t keyB = b2StoreBodyId(bodyB);
        bool found = false;
        float bestImpulse = -1.0f;
        for (int i = 0; i < count; ++i)
        {
            const b2ContactData& contact = contacts[static_cast<std::size_t>(i)];
            if (contact.manifold.pointCount <= 0)
            {
                continue;
            }

            if (!b2Shape_IsValid(contact.shapeIdA) || !b2Shape_IsValid(contact.shapeIdB))
            {
                continue;
            }

            const b2BodyId contactBodyA = b2Shape_GetBody(contact.shapeIdA);
            const b2BodyId contactBodyB = b2Shape_GetBody(contact.shapeIdB);
            if (!b2Body_IsValid(contactBodyA) || !b2Body_IsValid(contactBodyB))
            {
                continue;
            }

            const uint64_t contactKeyA = b2StoreBodyId(contactBodyA);
            const uint64_t contactKeyB = b2StoreBodyId(contactBodyB);
            const bool matches = (contactKeyA == keyA && contactKeyB == keyB)
                || (contactKeyA == keyB && contactKeyB == keyA);
            if (!matches)
            {
                continue;
            }

            for (int pointIndex = 0; pointIndex < contact.manifold.pointCount; ++pointIndex)
            {
                const b2ManifoldPoint& point = contact.manifold.points[pointIndex];
                const float impulse = point.totalNormalImpulse;
                if (!found || impulse > bestImpulse)
                {
                    found = true;
                    bestImpulse = impulse;
                    outPoint = point.point;
                }
            }
        }

        if (!found)
        {
            return false;
        }

        if (outImpulse)
        {
            *outImpulse = std::max(0.0f, bestImpulse);
        }

        return true;
    }

    SweetsContacts::WeakSweetsHandle SweetsContacts::FindSweetsHandle(ActiveSweets* sweets) const
    {
        if (!sweets)
        {
            return {};
        }

        auto it = m_sweetsHandleLookup.find(sweets);
        if (it == m_sweetsHandleLookup.end())
        {
            return {};
        }

        return it->second;
    }
}
