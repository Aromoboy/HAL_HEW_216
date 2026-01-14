#include "Sweets/SweetsContacts.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

#include "CollisionLayer.h"
#include "MaterialDX.h"
#include "ModelDX.h"
#include "SceneGameConfigTypes.h"
#include "Sweets/BaseSweets.h"
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
    constexpr float kCreamWeldBreakGraceSeconds = 0.25f;
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

}

namespace SweetsSpace
{
    SweetsContacts::SweetsContacts(Systems::PhysicsMachine* physicsMachine, bool* warnedMissingPhysicsMachine)
        : m_physicsMachine(physicsMachine)
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
            const JointPairKey key = JointPairKey::Make(cream, other);
            bool pendingCreated = false;
            float pendingElapsedSeconds = 0.0f;
            if (auto breakIt = m_pendingBreaks.find(key); breakIt != m_pendingBreaks.end())
            {
                m_pendingBreaks.erase(breakIt);
            }
            if (cream->IsRecentlyReleased() && m_creamReleaseCooldownSeconds > 0.0f)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "クリーム '" << cream->GetTag()
                    << "' は recently released だから接着をブロックするよ cooldown残り="
                    << cream->GetRecentlyReleasedSecondsRemaining() << "s\n";
                DebugTrace(oss.str());
            }
            else if (m_creamRequireCarried && !cream->WasEverCarried())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "クリーム '" << cream->GetTag()
                    << "' はまだ運ばれてないから接着をスキップするよ \n";
                DebugTrace(oss.str());
            }
            else if (IsAdhesionTargetAllowed(*cream, *other))
            {
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
                        pending.awaitingRestore = false;
                        pending.graceSecondsRemaining = 0.0f;
                        m_pendingAdhesions.emplace(key, pending);
                        pendingCreated = true;
                    }
                    else
                    {
                        if (ShouldLogCreamPairEvent(cream, other, "duplicateContactBegin"))
                        {
                            std::ostringstream oss;
                            oss << kLogPrefix << "duplicateContactBegin cream='" << cream->GetTag()
                                << "' other='" << other->GetTag() << "'\n";
                            LogInfoAndTrace(oss.str());
                        }
                        pendingIt->second.cream = cream;
                        pendingIt->second.other = other;
                        pendingIt->second.stillTouching = true;
                        pendingIt->second.awaitingRestore = false;
                        pendingIt->second.graceSecondsRemaining = 0.0f;
                        pendingElapsedSeconds = pendingIt->second.elapsedSeconds;
                    }
                }
            }

            if (auto pendingIt = m_pendingAdhesions.find(key); pendingIt != m_pendingAdhesions.end())
            {
                pendingElapsedSeconds = pendingIt->second.elapsedSeconds;
            }

            if (ShouldLogCreamPairEvent(cream, other, "contact_begin"))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "cream contact begin cream='" << cream->GetTag()
                    << "' other='" << other->GetTag()
                    << "' layer=" << static_cast<int>(other->GetCollisionLayer())
                    << " elapsed=" << pendingElapsedSeconds
                    << " pending_created=" << std::boolalpha << pendingCreated << std::noboolalpha
                    << "\n";
                LogInfoAndTrace(oss.str());
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

            if (auto pendingIt = m_pendingAdhesions.find(key); pendingIt != m_pendingAdhesions.end())
            {
                pendingIt->second.stillTouching = false;
                pendingIt->second.awaitingRestore = true;
                pendingIt->second.graceSecondsRemaining =
                    std::max(pendingIt->second.graceSecondsRemaining, m_creamAdhesionGraceSeconds);
            }

            if (auto it = m_creamWeldJoints.find(key); it != m_creamWeldJoints.end())
            {
                auto breakIt = m_pendingBreaks.find(key);
                if (breakIt == m_pendingBreaks.end())
                {
                    PendingBreak pending{};
                    pending.cream = cream;
                    pending.other = other;
                    pending.elapsedSeconds = 0.0f;
                    pending.stillSeparated = true;
                    m_pendingBreaks.emplace(key, pending);
                }
                else
                {
                    breakIt->second.cream = cream;
                    breakIt->second.other = other;
                    breakIt->second.elapsedSeconds = 0.0f;
                    breakIt->second.stillSeparated = true;
                }
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

        const float sanitizedSpeed = std::isfinite(approachSpeed) ? std::max(0.0f, approachSpeed) : 0.0f;
        const JointPairKey pairKey = JointPairKey::Make(a, b);
        if (auto it = m_contactHitImpulses.find(pairKey); it != m_contactHitImpulses.end())
        {
            it->second = std::max(it->second, sanitizedSpeed);
        }
        else
        {
            m_contactHitImpulses.emplace(pairKey, sanitizedSpeed);
        }

        if (m_creamBreakImpulseThreshold <= 0.0f || sanitizedSpeed < m_creamBreakImpulseThreshold)
        {
            return;
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

        if (!cream || !other)
        {
            return;
        }

        const JointPairKey weldKey = JointPairKey::Make(cream, other);
        if (m_creamWeldJoints.find(weldKey) == m_creamWeldJoints.end())
        {
            return;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "衝突でクリーム接着が外れたよ cream='" << cream->GetTag()
            << "' other='" << other->GetTag()
            << "' impact=" << sanitizedSpeed
            << " threshold=" << m_creamBreakImpulseThreshold << "\n";
        LogInfoAndTrace(oss.str());
        RequestCreamWeldBreak(cream, other, "ContactHit");
    }

    void SweetsContacts::OnUpdate(float dt)
    {
        CleanupInvalidContacts();

        if (!m_pendingCementClusterRebuilds.empty())
        {
            if (m_cementClusterRebuildDelayFrames > 0)
            {
                --m_cementClusterRebuildDelayFrames;
            }
            else
            {
                ProcessPendingCementClusterRebuilds();
            }
        }

        const bool checkBreakImpulse = m_creamWeldBreakImpulseThreshold > 0.0f;
        const bool hasPendingRebuilds =
            !m_pendingCementClusterRebuilds.empty() || m_cementClusterRebuildDelayFrames > 0;
        if (m_pendingAdhesions.empty() && m_pendingBreaks.empty() && m_pendingCementClusters.empty()
            && !checkBreakImpulse && !hasPendingRebuilds)
        {
            m_deferCementClusterRebuild = false;
            return;
        }

        if (dt <= 0.0f)
        {
            m_deferCementClusterRebuild = false;
            return;
        }

        const auto clearPendingForDeletion = [this](Entities::PhysicalEntity* entity)
        {
            if (!entity)
            {
                return;
            }

            ClearPendingAdhesionsForEntity(entity);
            ClearPendingCementClustersForEntity(entity);
            if (auto* sweets = dynamic_cast<ActiveSweets*>(entity))
            {
                ClearPendingBreaksForSweets(sweets);
            }
        };

        for (auto it = m_pendingAdhesions.begin(); it != m_pendingAdhesions.end();)
        {
            PendingAdhesion& pending = it->second;
            if (IsPendingDeletion(pending.cream) || IsPendingDeletion(pending.other))
            {
                clearPendingForDeletion(pending.cream);
                clearPendingForDeletion(pending.other);
                it = m_pendingAdhesions.erase(it);
                continue;
            }

            if (!pending.cream || !pending.other || !pending.stillTouching)
            {
                if (pending.cream && pending.other && !pending.stillTouching && pending.graceSecondsRemaining > 0.0f)
                {
                    pending.graceSecondsRemaining = std::max(0.0f, pending.graceSecondsRemaining - dt);
                    ++it;
                    continue;
                }

                it = m_pendingAdhesions.erase(it);
                continue;
            }

            pending.elapsedSeconds += dt;
            if (pending.elapsedSeconds < m_creamAdhesionDelaySeconds)
            {
                ++it;
                continue;
            }

            if (ShouldLogCreamPairEvent(pending.cream, pending.other, "adhesion_ready"))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "cream adhesion ready cream='" << pending.cream->GetTag()
                    << "' other='" << pending.other->GetTag()
                    << "' elapsed=" << pending.elapsedSeconds
                    << " delay=" << m_creamAdhesionDelaySeconds
                    << "\n";
                LogInfoAndTrace(oss.str());
            }

            if ((m_creamRequireCarried && !pending.cream->WasEverCarried())
                || !IsAdhesionTargetAllowed(*pending.cream, *pending.other))
            {
                it = m_pendingAdhesions.erase(it);
                continue;
            }

            TryCreateCreamWeldJoint(pending.cream, pending.other);
            it = m_pendingAdhesions.erase(it);
        }

        for (auto it = m_pendingCementClusters.begin(); it != m_pendingCementClusters.end();)
        {
            PendingCementCluster& pending = it->second;
            if (IsPendingDeletion(pending.cream) || IsPendingDeletion(pending.other))
            {
                clearPendingForDeletion(pending.cream);
                clearPendingForDeletion(pending.other);
                it = m_pendingCementClusters.erase(it);
                continue;
            }

            if (!HasValidBodyAndShapes(pending.cream) || !HasValidBodyAndShapes(pending.other))
            {
                it = m_pendingCementClusters.erase(it);
                continue;
            }

            pending.elapsedSeconds += dt;
            if (pending.elapsedSeconds < m_creamAdhesionDelaySeconds)
            {
                ++it;
                continue;
            }

            if (BuildCementCluster(pending.cream, pending.other))
            {
                it = m_pendingCementClusters.erase(it);
                continue;
            }

            ++it;
        }

        for (auto it = m_pendingBreaks.begin(); it != m_pendingBreaks.end();)
        {
            PendingBreak& pending = it->second;
            if (IsPendingDeletion(pending.cream) || IsPendingDeletion(pending.other))
            {
                clearPendingForDeletion(pending.cream);
                clearPendingForDeletion(pending.other);
                it = m_pendingBreaks.erase(it);
                continue;
            }

            if (!pending.cream || !pending.other || !pending.stillSeparated)
            {
                it = m_pendingBreaks.erase(it);
                continue;
            }

            const JointPairKey key = JointPairKey::Make(pending.cream, pending.other);
            if (m_creamWeldJoints.find(key) == m_creamWeldJoints.end())
            {
                it = m_pendingBreaks.erase(it);
                continue;
            }

            pending.elapsedSeconds += dt;
            if (pending.elapsedSeconds < kCreamWeldBreakGraceSeconds)
            {
                ++it;
                continue;
            }

            BreakCreamWeldJoint(pending.cream, pending.other, "OnUpdate");
            it = m_pendingBreaks.erase(it);
        }

        if (checkBreakImpulse && m_physicsMachine && !m_creamWeldJoints.empty())
        {
            std::vector<std::pair<JointPairKey, float>> jointsToBreak;
            jointsToBreak.reserve(m_creamWeldJoints.size());

            for (const auto& pair : m_creamWeldJoints)
            {
                const b2JointId jointId = pair.second;
                if (!b2Joint_IsValid(jointId))
                {
                    continue;
                }

                const b2Vec2 force = b2Joint_GetConstraintForce(jointId);
                const float forceMagnitude = std::sqrt((force.x * force.x) + (force.y * force.y));
                const float impulse = forceMagnitude * dt;
                if (impulse >= m_creamWeldBreakImpulseThreshold)
                {
                    jointsToBreak.emplace_back(pair.first, impulse);
                }
            }

            for (const auto& entry : jointsToBreak)
            {
                const JointPairKey& key = entry.first;
                const float impulse = entry.second;
                Entities::PhysicalEntity* first = key.first;
                Entities::PhysicalEntity* second = key.second;
                auto* cream = dynamic_cast<ActiveSweets*>(first);
                Entities::PhysicalEntity* other = second;
                if (!cream || !IsCreamTag(cream->GetTag()))
                {
                    cream = dynamic_cast<ActiveSweets*>(second);
                    other = first;
                }

                if (!cream || !other)
                {
                    continue;
                }

                std::ostringstream oss;
                oss << kLogPrefix << "クリーム接着の破断閾値を超えたよ cream='"
                    << cream->GetTag() << "' other='" << other->GetTag()
                    << "' threshold=" << m_creamWeldBreakImpulseThreshold
                    << " impulse=" << impulse << "\n";
                LogInfoAndTrace(oss.str());

                BreakCreamWeldJoint(cream, other, "BreakImpulseThreshold");
            }
        }

        m_deferCementClusterRebuild = false;
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
        ClearHitImpulsesForSweets(sweets);
    }

    void SweetsContacts::ClearSweetsContact(ActiveSweets* sweets)
    {
        if (!sweets)
        {
            return;
        }

        m_hasSweetsContact.erase(sweets);
        ClearHitImpulsesForSweets(sweets);
    }

    void SweetsContacts::ClearPendingAdhesionsForSweets(ActiveSweets* sweets)
    {
        ClearPendingAdhesionsForEntity(sweets);
    }

    void SweetsContacts::ClearPendingAdhesionsForEntity(Entities::PhysicalEntity* entity)
    {
        if (!entity || m_pendingAdhesions.empty())
        {
            return;
        }

        for (auto it = m_pendingAdhesions.begin(); it != m_pendingAdhesions.end();)
        {
            if (it->second.cream == entity || it->second.other == entity)
            {
                it = m_pendingAdhesions.erase(it);
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

        BreakCreamWeldJoint(cream, other, context ? context : "RequestCreamWeldBreak");
        const JointPairKey key = JointPairKey::Make(cream, other);
        if (auto it = m_pendingBreaks.find(key); it != m_pendingBreaks.end())
        {
            m_pendingBreaks.erase(it);
        }
    }

    void SweetsContacts::Reset()
    {
        m_contactStates.clear();
        m_hasSweetsContact.clear();
        m_contactHitImpulses.clear();
        ClearCreamWeldJoints("Reset");
        ClearCementClusters("Reset");
        m_sweetsHandleLookup.clear();
        m_pendingAdhesions.clear();
        m_pendingBreaks.clear();
        m_pendingCementClusters.clear();
        m_pendingCementClusterRebuilds.clear();
        m_cementClusterRebuildDelayFrames = 0;
        m_deferCementClusterRebuild = false;
        m_loggedCreamPairEvents.clear();
        m_loggedContactNotification = false;
    }

    void SweetsContacts::NotifyUnregisterOrRetry()
    {
        DeferCementClusterRebuild();
    }

    void SweetsContacts::RemoveCreamWeldJointsForSweets(ActiveSweets* sweets, const char* context)
    {
        if (!sweets)
        {
            return;
        }

        ClearPendingAdhesionsForSweets(sweets);
        ClearPendingCementClustersForEntity(sweets);
        ClearPendingBreaksForSweets(sweets);
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

    void SweetsContacts::ClearPendingBreaksForSweets(ActiveSweets* sweets)
    {
        if (!sweets || m_pendingBreaks.empty())
        {
            return;
        }

        for (auto it = m_pendingBreaks.begin(); it != m_pendingBreaks.end();)
        {
            if (it->second.cream == sweets || it->second.other == sweets)
            {
                it = m_pendingBreaks.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void SweetsContacts::ClearPendingCementClustersForEntity(Entities::PhysicalEntity* entity)
    {
        if (!entity)
        {
            return;
        }

        for (auto it = m_pendingCementClusters.begin(); it != m_pendingCementClusters.end();)
        {
            if (it->second.cream == entity || it->second.other == entity)
            {
                it = m_pendingCementClusters.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = m_pendingCementClusterRebuilds.begin(); it != m_pendingCementClusterRebuilds.end();)
        {
            const JointPairKey& key = *it;
            if (key.first == entity || key.second == entity)
            {
                it = m_pendingCementClusterRebuilds.erase(it);
            }
            else
            {
                ++it;
            }
        }

        if (m_pendingCementClusterRebuilds.empty())
        {
            m_cementClusterRebuildDelayFrames = 0;
        }
    }

    void SweetsContacts::ClearHitImpulsesForSweets(ActiveSweets* sweets)
    {
        if (!sweets || m_contactHitImpulses.empty())
        {
            return;
        }

        for (auto it = m_contactHitImpulses.begin(); it != m_contactHitImpulses.end();)
        {
            if (it->first.first == sweets || it->first.second == sweets)
            {
                it = m_contactHitImpulses.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    void SweetsContacts::CleanupInvalidContacts()
    {
        PruneCementGraph();

        for (auto it = m_contactStates.begin(); it != m_contactStates.end();)
        {
            if (!HasValidBodyAndShapes(it->first))
            {
                m_hasSweetsContact.erase(it->first);
                it = m_contactStates.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = m_pendingAdhesions.begin(); it != m_pendingAdhesions.end();)
        {
            if (!HasValidBodyAndShapes(it->second.cream) || !HasValidBodyAndShapes(it->second.other))
            {
                it = m_pendingAdhesions.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = m_pendingCementClusters.begin(); it != m_pendingCementClusters.end();)
        {
            if (!HasValidBodyAndShapes(it->second.cream) ||
                !HasValidBodyAndShapes(it->second.other))
            {
                it = m_pendingCementClusters.erase(it);
            }
            else
            {
                ++it;
            }
        }
    };

    void SweetsContacts::ClearCreamWeldJoints(const char* context)
    {
        if (m_creamWeldJoints.empty())
        {
            m_pendingAdhesions.clear();
            m_pendingBreaks.clear();
            return;
        }

        m_pendingAdhesions.clear();
        m_pendingBreaks.clear();
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

    void SweetsContacts::RemoveCementClusterForSweets(ActiveSweets* sweets, const char* context)
    {
        if (!sweets)
        {
            return;
        }

        auto it = m_cementClusterLookup.find(sweets);
        if (it != m_cementClusterLookup.end())
        {
            RemoveCementCluster(it->second, context ? context : "RemoveCementClusterForSweets");
        }

        RemoveCementAdjacencyForEntity(sweets);
    }

    void SweetsContacts::ClearCementClusters(const char* context)
    {
        if (m_cementClusters.empty())
        {
            m_cementAdjacency.clear();
            m_cementCakeAdjacency.clear();
            m_cementCakeEntity = nullptr;
            m_cementClusterLookup.clear();
            return;
        }

        std::vector<std::shared_ptr<CementCluster>> clusters;
        clusters.reserve(m_cementClusters.size());
        for (const auto& cluster : m_cementClusters)
        {
            clusters.push_back(cluster);
        }

        for (const auto& cluster : clusters)
        {
            RemoveCementCluster(cluster, context ? context : "ClearCementClusters");
        }

        m_cementAdjacency.clear();
        m_cementCakeAdjacency.clear();
        m_cementCakeEntity = nullptr;
        m_cementClusterLookup.clear();
        m_cementClusters.clear();
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

    bool SweetsContacts::IsAdhesionTargetAllowed(const ActiveSweets& cream, const Entities::PhysicalEntity& other) const
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
            logRejectedTarget("blocked tag");
            return false;
        }

        if (IsCreamTag(other.GetTag()))
        {
            logRejectedTarget("cream target");
            return false;
        }

        const bool isSweetsTarget = dynamic_cast<const ActiveSweets*>(&other) != nullptr;
        const Collision::CollisionLayer otherLayer = other.GetCollisionLayer();
        if (!isSweetsTarget && otherLayer != Collision::CollisionLayer::Stage)
        {
            logRejectedTarget("non-sweets");
            return false;
        }

        std::vector<std::string> keys;
        const std::string tagKey = NormalizeKey(other.GetTag());
        if (otherLayer == Collision::CollisionLayer::Stage && tagKey != kStageCakeKey)
        {
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
                logRejectedTarget("not in allowed list");
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
            return;
        }

        if (!IsAdhesionTargetAllowed(*cream, *other))
        {
            if (ShouldLogCreamPairEvent(cream, other, "target_disallowed"))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "cream weld early return reason=target disallowed cream='"
                    << cream->GetTag() << "' other='" << other->GetTag() << "'\n";
                LogInfoAndTrace(oss.str());
            }
            return;
        }

        const bool creamLocked = IsCementedOrLocked(*cream);
        const bool otherLocked = IsCementedOrLocked(*other);
        const bool creamSupported = HasSupportingContact(*cream, nullptr);
        bool otherSupportedForAdhesion = false;
        bool otherSupportedForSleep = true;
        const JointPairKey key = JointPairKey::Make(cream, other);
        float pendingElapsedSeconds = 0.0f;
        if (const auto pendingIt = m_pendingAdhesions.find(key); pendingIt != m_pendingAdhesions.end())
        {
            pendingElapsedSeconds = pendingIt->second.elapsedSeconds;
        }
        const bool pendingLongEnough = pendingElapsedSeconds >= m_creamAdhesionDelaySeconds;
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
            return;
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
            return;
        }
        auto existing = m_creamWeldJoints.find(key);
        if (existing != m_creamWeldJoints.end() && !b2Joint_IsValid(existing->second))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "壊れたクリーム接着ジョイントを掃除したよ cream='"
                << cream->GetTag() << "' other='" << other->GetTag() << "'\n";
            LogInfoAndTrace(oss.str());
            m_creamWeldJoints.erase(existing);
            existing = m_creamWeldJoints.end();
        }

        if (existing != m_creamWeldJoints.end())
        {
            if (ShouldLogCreamPairEvent(cream, other, "existing_joint"))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "cream weld early return reason=existing joint cream='"
                    << cream->GetTag() << "' other='" << other->GetTag() << "'\n";
                LogInfoAndTrace(oss.str());
            }
            return;
        }

        const auto& posA = cream->GetTransform().position;
        const auto& posB = other->GetTransform().position;
        const b2Vec2 anchor{ 0.5f * (posA.x + posB.x), 0.5f * (posA.y + posB.y) };
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

        const bool shouldSleep = creamSupported && otherSupportedForSleep;
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
        LogInfoAndTrace(cementLog.str());

        if (auto pendingIt = m_pendingCementClusters.find(key); pendingIt != m_pendingCementClusters.end())
        {
            m_pendingCementClusters.erase(pendingIt);
        }
        LogInfoAndTrace(std::string(kLogPrefix) + "cement cluster disabled\n");
    }

    void SweetsContacts::BreakCreamWeldJoint(ActiveSweets* cream, Entities::PhysicalEntity* other, const char* context)
    {
        if (!cream || !other)
        {
            return;
        }

        const JointPairKey key = JointPairKey::Make(cream, other);
        auto it = m_creamWeldJoints.find(key);
        if (it == m_creamWeldJoints.end())
        {
            return;
        }
        if (auto pendingIt = m_pendingCementClusters.find(key); pendingIt != m_pendingCementClusters.end())
        {
            m_pendingCementClusters.erase(pendingIt);
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
        else if (b2Joint_IsValid(it->second))
        {
            m_physicsMachine->DestroyJoint(it->second);
            std::ostringstream cementLog;
            cementLog << kLogPrefix << "cementの溶接を外したよ '" << cream->GetTag() << "' と '"
                      << other->GetTag() << "'\n";
            DebugTrace(cementLog.str());
        }

        m_creamWeldJoints.erase(it);
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

    bool SweetsContacts::UpdateCementAdjacencyBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b)
    {
        if (!a || !b)
        {
            return false;
        }

        const bool aValid = HasValidBodyAndShapes(a);
        const bool bValid = HasValidBodyAndShapes(b);
        if (!aValid || !bValid)
        {
            if (!aValid)
            {
                RemoveCementAdjacencyForEntity(a);
            }
            if (!bValid)
            {
                RemoveCementAdjacencyForEntity(b);
            }
            return false;
        }

        if (IsPendingDeletion(a) || IsPendingDeletion(b))
        {
            return false;
        }

        const bool aIsCake = IsCakeEntity(*a);
        const bool bIsCake = IsCakeEntity(*b);
        auto* sweetsA = dynamic_cast<ActiveSweets*>(a);
        auto* sweetsB = dynamic_cast<ActiveSweets*>(b);

        if (sweetsA && sweetsB)
        {
            if (!IsCementClusterCandidate(*sweetsA) || !IsCementClusterCandidate(*sweetsB))
            {
                return false;
            }

            const auto handleA = FindSweetsHandle(sweetsA);
            const auto handleB = FindSweetsHandle(sweetsB);
            if (handleA.expired() || handleB.expired())
            {
                RemoveCementAdjacencyForEntity(sweetsA);
                RemoveCementAdjacencyForEntity(sweetsB);
                return false;
            }

            const auto itA = m_cementAdjacency.find(handleA);
            if (itA != m_cementAdjacency.end() && itA->second.find(handleB) != itA->second.end())
            {
                return false;
            }

            const auto itB = m_cementAdjacency.find(handleB);
            if (itB != m_cementAdjacency.end() && itB->second.find(handleA) != itB->second.end())
            {
                return false;
            }

            m_cementAdjacency[handleA].insert(handleB);
            m_cementAdjacency[handleB].insert(handleA);
            return true;
        }

        if (sweetsA && bIsCake)
        {
            if (!IsCementClusterCandidate(*sweetsA))
            {
                return false;
            }

            const auto handleA = FindSweetsHandle(sweetsA);
            if (handleA.expired())
            {
                RemoveCementAdjacencyForEntity(sweetsA);
                return false;
            }

            auto& cakeSet = m_cementCakeAdjacency[kStageCakeKey];
            if (cakeSet.find(handleA) != cakeSet.end())
            {
                return false;
            }

            cakeSet.insert(handleA);
            m_cementCakeEntity = b;
            return true;
        }

        if (sweetsB && aIsCake)
        {
            if (!IsCementClusterCandidate(*sweetsB))
            {
                return false;
            }

            const auto handleB = FindSweetsHandle(sweetsB);
            if (handleB.expired())
            {
                RemoveCementAdjacencyForEntity(sweetsB);
                return false;
            }

            auto& cakeSet = m_cementCakeAdjacency[kStageCakeKey];
            if (cakeSet.find(handleB) != cakeSet.end())
            {
                return false;
            }

            cakeSet.insert(handleB);
            m_cementCakeEntity = a;
            return true;
        }

        return false;
    }

    void SweetsContacts::UpdateCementAdjacencyEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b)
    {
        if (!a || !b)
        {
            return;
        }

        const bool aValid = HasValidBodyAndShapes(a);
        const bool bValid = HasValidBodyAndShapes(b);
        if (!aValid || !bValid)
        {
            if (!aValid)
            {
                RemoveCementAdjacencyForEntity(a);
            }
            if (!bValid)
            {
                RemoveCementAdjacencyForEntity(b);
            }
            return;
        }

        auto* sweetsA = dynamic_cast<ActiveSweets*>(a);
        auto* sweetsB = dynamic_cast<ActiveSweets*>(b);
        const bool aIsCake = IsCakeEntity(*a);
        const bool bIsCake = IsCakeEntity(*b);

        if (sweetsA && sweetsB)
        {
            const auto handleA = FindSweetsHandle(sweetsA);
            const auto handleB = FindSweetsHandle(sweetsB);
            if (!handleA.expired())
            {
                auto itA = m_cementAdjacency.find(handleA);
                if (itA != m_cementAdjacency.end())
                {
                    itA->second.erase(handleB);
                    if (itA->second.empty())
                    {
                        m_cementAdjacency.erase(itA);
                    }
                }
            }

            if (!handleB.expired())
            {
                auto itB = m_cementAdjacency.find(handleB);
                if (itB != m_cementAdjacency.end())
                {
                    itB->second.erase(handleA);
                    if (itB->second.empty())
                    {
                        m_cementAdjacency.erase(itB);
                    }
                }
            }
            return;
        }

        if (sweetsA && bIsCake)
        {
            const auto handleA = FindSweetsHandle(sweetsA);
            if (auto cakeIt = m_cementCakeAdjacency.find(kStageCakeKey);
                cakeIt != m_cementCakeAdjacency.end())
            {
                cakeIt->second.erase(handleA);
                if (cakeIt->second.empty())
                {
                    m_cementCakeAdjacency.erase(cakeIt);
                }
            }
        }
        else if (sweetsB && aIsCake)
        {
            const auto handleB = FindSweetsHandle(sweetsB);
            if (auto cakeIt = m_cementCakeAdjacency.find(kStageCakeKey);
                cakeIt != m_cementCakeAdjacency.end())
            {
                cakeIt->second.erase(handleB);
                if (cakeIt->second.empty())
                {
                    m_cementCakeAdjacency.erase(cakeIt);
                }
            }
        }

        if (m_cementCakeAdjacency.empty())
        {
            m_cementCakeEntity = nullptr;
        }
    }

    void SweetsContacts::RequestCementClusterRebuild(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b)
    {
        if (!a || !b)
        {
            return;
        }

        if (!IsCementClusterCandidate(*a) || !IsCementClusterCandidate(*b))
        {
            return;
        }

        m_pendingCementClusterRebuilds.insert(JointPairKey::Make(a, b));
        const int requestedDelay = 1 + (m_deferCementClusterRebuild ? 1 : 0);
        m_cementClusterRebuildDelayFrames = std::max(m_cementClusterRebuildDelayFrames, requestedDelay);
    }

    void SweetsContacts::ProcessPendingCementClusterRebuilds()
    {
        if (m_pendingCementClusterRebuilds.empty())
        {
            return;
        }

        std::vector<JointPairKey> rebuilds;
        rebuilds.reserve(m_pendingCementClusterRebuilds.size());
        for (const auto& key : m_pendingCementClusterRebuilds)
        {
            rebuilds.push_back(key);
        }
        m_pendingCementClusterRebuilds.clear();
        m_cementClusterRebuildDelayFrames = 0;

        for (const auto& key : rebuilds)
        {
            Entities::PhysicalEntity* first = key.first;
            Entities::PhysicalEntity* second = key.second;
            auto* sweets = dynamic_cast<ActiveSweets*>(first);
            Entities::PhysicalEntity* other = second;
            if (!sweets)
            {
                sweets = dynamic_cast<ActiveSweets*>(second);
                other = first;
            }

            if (!sweets || !other)
            {
                continue;
            }

            if (IsPendingDeletion(sweets) || IsPendingDeletion(other))
            {
                continue;
            }

            BuildCementCluster(sweets, other);
        }
    }

    void SweetsContacts::DeferCementClusterRebuild()
    {
        m_deferCementClusterRebuild = true;
        if (m_pendingCementClusterRebuilds.empty())
        {
            return;
        }

        m_cementClusterRebuildDelayFrames = std::max(m_cementClusterRebuildDelayFrames, 1) + 1;
    }

    void SweetsContacts::RemoveCementAdjacencyForEntity(Entities::PhysicalEntity* entity)
    {
        if (!entity)
        {
            return;
        }

        PruneCementGraph();

        if (IsCakeEntity(*entity))
        {
            m_cementCakeAdjacency.clear();
            m_cementCakeEntity = nullptr;
            return;
        }

        auto* sweets = dynamic_cast<ActiveSweets*>(entity);
        if (!sweets)
        {
            return;
        }

        const auto handle = FindSweetsHandle(sweets);
        if (handle.expired())
        {
            return;
        }

        RemoveCementAdjacencyNode(handle);
        RemoveCakeAdjacencyNode(handle);
    }

    std::shared_ptr<SweetsContacts::CementCluster> SweetsContacts::BuildCementCluster(ActiveSweets* cream, Entities::PhysicalEntity* other)
    {
        if (!cream || !other)
        {
            return nullptr;
        }

        if (!IsCementClusterCandidate(*cream) || !IsCementClusterCandidate(*other))
        {
            return nullptr;
        }

        if (!HasValidBodyAndShapes(cream) || !HasValidBodyAndShapes(other))
        {
            return nullptr;
        }

        PruneCementGraph();

        std::unordered_set<std::shared_ptr<ActiveSweets>> visited;
        std::queue<WeakSweetsHandle> frontier;

        const auto creamHandle = FindSweetsHandle(cream);
        if (creamHandle.expired())
        {
            RemoveCementAdjacencyForEntity(cream);
            return nullptr;
        }
        if (auto creamLocked = creamHandle.lock())
        {
            visited.insert(creamLocked);
            frontier.push(creamHandle);
        }
        else
        {
            RemoveCementAdjacencyNode(creamHandle);
            return nullptr;
        }

        if (auto* otherSweets = dynamic_cast<ActiveSweets*>(other))
        {
            const auto otherHandle = FindSweetsHandle(otherSweets);
            if (otherHandle.expired())
            {
                RemoveCementAdjacencyForEntity(otherSweets);
                return nullptr;
            }
            if (auto otherLocked = otherHandle.lock())
            {
                visited.insert(otherLocked);
                frontier.push(otherHandle);
            }
            else
            {
                RemoveCementAdjacencyNode(otherHandle);
                return nullptr;
            }
        }

        while (!frontier.empty())
        {
            const auto currentHandle = frontier.front();
            frontier.pop();

            auto currentLocked = currentHandle.lock();
            if (!currentLocked)
            {
                RemoveCementAdjacencyNode(currentHandle);
                continue;
            }

            if (!HasValidBodyAndShapes(currentLocked.get()) || IsPendingDeletion(currentLocked.get()))
            {
                RemoveCementAdjacencyNode(currentHandle);
                continue;
            }

            auto adjacencyIt = m_cementAdjacency.find(currentHandle);
            if (adjacencyIt == m_cementAdjacency.end())
            {
                continue;
            }

            for (auto neighborIt = adjacencyIt->second.begin(); neighborIt != adjacencyIt->second.end();)
            {
                const auto neighborHandle = *neighborIt;
                auto neighborLocked = neighborHandle.lock();
                if (!neighborLocked)
                {
                    neighborIt = adjacencyIt->second.erase(neighborIt);
                    RemoveCementAdjacencyNode(neighborHandle);
                    continue;
                }

                if (!HasValidBodyAndShapes(neighborLocked.get()) || IsPendingDeletion(neighborLocked.get()))
                {
                    neighborIt = adjacencyIt->second.erase(neighborIt);
                    RemoveCementAdjacencyNode(neighborHandle);
                    continue;
                }

                const auto insertResult = visited.insert(neighborLocked);
                if (insertResult.second)
                {
                    frontier.push(neighborHandle);
                }

                ++neighborIt;
            }
        }

        std::unordered_set<Entities::PhysicalEntity*> members;
        members.reserve(visited.size());
        for (const auto& member : visited)
        {
            members.insert(member.get());
        }

        std::shared_ptr<CementCluster> targetCluster;
        std::unordered_set<std::shared_ptr<CementCluster>> clustersToRemove;
        std::unordered_set<ActiveSweets*> sweetsSet;
        Entities::PhysicalEntity* anchor = nullptr;
        bool anchoredToCake = false;

        if (m_cementCakeEntity && !HasValidBodyAndShapes(m_cementCakeEntity))
        {
            m_cementCakeEntity = nullptr;
        }

        for (const auto& member : visited)
        {
            auto* sweets = member.get();
            sweetsSet.insert(sweets);
            auto it = m_cementClusterLookup.find(sweets);
            if (it != m_cementClusterLookup.end())
            {
                if (!targetCluster)
                {
                    targetCluster = it->second;
                }
                clustersToRemove.insert(it->second);
            }

            if (!anchoredToCake && m_cementCakeEntity)
            {
                const WeakSweetsHandle handle = member;
                auto cakeIt = m_cementCakeAdjacency.find(kStageCakeKey);
                if (cakeIt != m_cementCakeAdjacency.end()
                    && cakeIt->second.find(handle) != cakeIt->second.end())
                {
                    anchor = m_cementCakeEntity;
                    anchoredToCake = true;
                }
            }
        }

        if (!anchor)
        {
            if (sweetsSet.find(cream) != sweetsSet.end())
            {
                anchor = cream;
            }
            else if (!sweetsSet.empty())
            {
                anchor = *sweetsSet.begin();
            }
            else
            {
                anchor = other;
            }
        }

        if (!anchor)
        {
            return nullptr;
        }

        if (targetCluster)
        {
            for (const auto& existing : clustersToRemove)
            {
                if (existing != targetCluster)
                {
                    RemoveCementCluster(existing, "CementClusterMerge");
                }
            }

            RemoveCementCluster(targetCluster, "CementClusterUpdate");
            targetCluster->joints.clear();
            targetCluster->members = members;
            targetCluster->sweets = std::move(sweetsSet);
            targetCluster->anchor = anchor;
            targetCluster->anchoredToCake = anchoredToCake;
            CreateCementClusterJoints(*targetCluster, "CementClusterUpdate");
            m_cementClusters.insert(targetCluster);
            for (auto* sweets : targetCluster->sweets)
            {
                m_cementClusterLookup[sweets] = targetCluster;
            }

            std::ostringstream oss;
            oss << kLogPrefix << "cement クラスター更新 size=" << targetCluster->sweets.size()
                << " members=" << targetCluster->members.size()
                << " anchor='" << targetCluster->anchor->GetTag() << "'"
                << " anchoredToCake=" << std::boolalpha << targetCluster->anchoredToCake << std::noboolalpha
                << "\n";
            LogInfoAndTrace(oss.str());

            return targetCluster;
        }

        auto cluster = std::make_shared<CementCluster>();
        cluster->members = members;
        cluster->sweets = std::move(sweetsSet);
        cluster->anchor = anchor;
        cluster->anchoredToCake = anchoredToCake;

        CreateCementClusterJoints(*cluster, "CementClusterCreate");
        m_cementClusters.insert(cluster);
        for (auto* sweets : cluster->sweets)
        {
            m_cementClusterLookup[sweets] = cluster;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "cement クラスター作成 size=" << cluster->sweets.size()
            << " members=" << cluster->members.size()
            << " anchor='" << cluster->anchor->GetTag() << "'"
            << " anchoredToCake=" << std::boolalpha << cluster->anchoredToCake << std::noboolalpha
            << "\n";
        LogInfoAndTrace(oss.str());

        return cluster;
    }

    void SweetsContacts::RemoveCementCluster(const std::shared_ptr<CementCluster>& cluster, const char* context)
    {
        if (!cluster)
        {
            return;
        }

        if (!m_physicsMachine)
        {
            if (m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
            {
                std::ostringstream oss;
                oss << kLogPrefix << (context ? context : "RemoveCementCluster")
                    << " でcementクラスターを外せなかったよ 物理マシン未設定 \n";
                DebugTrace(oss.str());
                *m_warnedMissingPhysicsMachine = true;
            }
        }
        else
        {
            for (const auto& jointId : cluster->joints)
            {
                if (b2Joint_IsValid(jointId))
                {
                    m_physicsMachine->DestroyJoint(jointId);
                }
            }
        }

        for (auto* sweets : cluster->sweets)
        {
            auto it = m_cementClusterLookup.find(sweets);
            if (it != m_cementClusterLookup.end() && it->second == cluster)
            {
                m_cementClusterLookup.erase(it);
            }
        }

        m_cementClusters.erase(cluster);
    }

    void SweetsContacts::CreateCementClusterJoints(CementCluster& cluster, const char* context)
    {
        if (!m_physicsMachine || !cluster.anchor)
        {
            if (!m_physicsMachine && m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
            {
                std::ostringstream oss;
                oss << kLogPrefix << (context ? context : "CreateCementClusterJoints")
                    << " でcementクラスターの溶接に失敗したよ 物理マシン未設定 \n";
                DebugTrace(oss.str());
                *m_warnedMissingPhysicsMachine = true;
            }
            return;
        }

        for (auto* member : cluster.members)
        {
            if (!member || member == cluster.anchor)
            {
                continue;
            }

            const JointPairKey key = JointPairKey::Make(cluster.anchor, member);
            if (m_creamWeldJoints.find(key) != m_creamWeldJoints.end())
            {
                continue;
            }

            const auto& posA = cluster.anchor->GetTransform().position;
            const auto& posB = member->GetTransform().position;
            const b2Vec2 anchor{ 0.5f * (posA.x + posB.x), 0.5f * (posA.y + posB.y) };
            const b2JointId jointId = m_physicsMachine->CreateWeldJoint(
                *cluster.anchor,
                *member,
                anchor,
                m_creamWeldLinearHertz,
                m_creamWeldLinearDampingRatio,
                m_creamWeldAngularHertz,
                m_creamWeldAngularDampingRatio,
                false);
            if (!b2Joint_IsValid(jointId))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "cementクラスター溶接に失敗したよ anchor='"
                    << cluster.anchor->GetTag() << "' member='" << member->GetTag() << "'\n";
                LogInfoAndTrace(oss.str());
                continue;
            }

            cluster.joints.push_back(jointId);

            if (cluster.anchor->GetPhysical().IsDynamic())
            {
                cluster.anchor->SetLinearVelocity(float3{ 0.0f, 0.0f, 0.0f });
                cluster.anchor->SetAngularVelocity(0.0f);
            }

            if (member->GetPhysical().IsDynamic())
            {
                member->SetLinearVelocity(float3{ 0.0f, 0.0f, 0.0f });
                member->SetAngularVelocity(0.0f);
            }
        }
    }

    bool SweetsContacts::IsCementClusterCandidate(const Entities::PhysicalEntity& entity) const
    {
        if (IsBlockedCreamTargetTag(entity.GetTag()))
        {
            return false;
        }

        if (dynamic_cast<const ActiveSweets*>(&entity))
        {
            return true;
        }

        return IsCakeEntity(entity);
    }

    bool SweetsContacts::IsCakeEntity(const Entities::PhysicalEntity& entity) const
    {
        if (entity.GetCollisionLayer() != Collision::CollisionLayer::Stage)
        {
            return false;
        }

        return NormalizeKey(entity.GetTag()) == kStageCakeKey;
    }

    bool SweetsContacts::IsCementedOrLocked(const Entities::PhysicalEntity& entity) const
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

    void SweetsContacts::PruneCementGraph()
    {
        std::size_t removedAdjacencyNodes = 0;
        std::size_t removedAdjacencyLinks = 0;
        const auto eraseByOwner = [](WeakSweetsSet& set, const WeakSweetsHandle& handle)
        {
            std::size_t removed = 0;
            for (auto it = set.begin(); it != set.end();)
            {
                if (WeakSweetsEqual{}(*it, handle))
                {
                    it = set.erase(it);
                    ++removed;
                }
                else
                {
                    ++it;
                }
            }
            return removed;
        };

        for (auto it = m_sweetsHandleLookup.begin(); it != m_sweetsHandleLookup.end();)
        {
            if (it->second.expired())
            {
                it = m_sweetsHandleLookup.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = m_cementAdjacency.begin(); it != m_cementAdjacency.end();)
        {
            const auto currentHandle = it->first;
            auto currentLocked = currentHandle.lock();
            if (!currentLocked || !HasValidBodyAndShapes(currentLocked.get()) || IsPendingDeletion(currentLocked.get()))
            {
                for (const auto& neighborHandle : it->second)
                {
                    auto neighborIt = m_cementAdjacency.find(neighborHandle);
                    if (neighborIt != m_cementAdjacency.end())
                    {
                        const std::size_t erased = eraseByOwner(neighborIt->second, currentHandle);
                        removedAdjacencyLinks += erased;
                        if (neighborIt->second.empty())
                        {
                            m_cementAdjacency.erase(neighborIt);
                            ++removedAdjacencyNodes;
                        }
                    }
                }

                removedAdjacencyLinks += it->second.size();
                it = m_cementAdjacency.erase(it);
                ++removedAdjacencyNodes;
                continue;
            }

            for (auto neighborIt = it->second.begin(); neighborIt != it->second.end();)
            {
                const auto neighborHandle = *neighborIt;
                auto neighborLocked = neighborHandle.lock();
                if (!neighborLocked || !HasValidBodyAndShapes(neighborLocked.get())
                    || IsPendingDeletion(neighborLocked.get()))
                {
                    neighborIt = it->second.erase(neighborIt);
                    ++removedAdjacencyLinks;
                    auto reverseIt = m_cementAdjacency.find(neighborHandle);
                    if (reverseIt != m_cementAdjacency.end())
                    {
                        const std::size_t erased = eraseByOwner(reverseIt->second, currentHandle);
                        removedAdjacencyLinks += erased;
                        if (reverseIt->second.empty())
                        {
                            m_cementAdjacency.erase(reverseIt);
                            ++removedAdjacencyNodes;
                        }
                    }
                }
                else
                {
                    ++neighborIt;
                }
            }

            if (it->second.empty())
            {
                it = m_cementAdjacency.erase(it);
                ++removedAdjacencyNodes;
            }
            else
            {
                ++it;
            }
        }

        for (auto mapIt = m_cementCakeAdjacency.begin(); mapIt != m_cementCakeAdjacency.end();)
        {
            auto& cakeSet = mapIt->second;
            for (auto it = cakeSet.begin(); it != cakeSet.end();)
            {
                auto locked = it->lock();
                if (!locked || !HasValidBodyAndShapes(locked.get()) || IsPendingDeletion(locked.get()))
                {
                    it = cakeSet.erase(it);
                    ++removedAdjacencyLinks;
                }
                else
                {
                    ++it;
                }
            }

            if (cakeSet.empty())
            {
                mapIt = m_cementCakeAdjacency.erase(mapIt);
            }
            else
            {
                ++mapIt;
            }
        }

        if (m_cementCakeAdjacency.empty())
        {
            m_cementCakeEntity = nullptr;
        }
        else if (m_cementCakeEntity && !HasValidBodyAndShapes(m_cementCakeEntity))
        {
            m_cementCakeEntity = nullptr;
        }

        if (removedAdjacencyNodes > 0 || removedAdjacencyLinks > 0)
        {
            std::ostringstream oss;
            oss << kLogPrefix
                << "PruneCementGraph で cement adjacency ノードを "
                << removedAdjacencyNodes << " 件削除したよ"
                << " link=" << removedAdjacencyLinks
                << "\n";
            DebugTrace(oss.str());
        }
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

    void SweetsContacts::RemoveCementAdjacencyNode(const WeakSweetsHandle& handle)
    {
        auto it = m_cementAdjacency.find(handle);
        if (it == m_cementAdjacency.end())
        {
            for (auto scanIt = m_cementAdjacency.begin(); scanIt != m_cementAdjacency.end(); ++scanIt)
            {
                if (WeakSweetsEqual{}(scanIt->first, handle))
                {
                    it = scanIt;
                    break;
                }
            }
        }

        if (it == m_cementAdjacency.end())
        {
            return;
        }

        const auto eraseByOwner = [&](WeakSweetsSet& set)
        {
            for (auto setIt = set.begin(); setIt != set.end();)
            {
                if (WeakSweetsEqual{}(*setIt, handle))
                {
                    setIt = set.erase(setIt);
                }
                else
                {
                    ++setIt;
                }
            }
        };

        for (const auto& neighborHandle : it->second)
        {
            auto neighborIt = m_cementAdjacency.find(neighborHandle);
            if (neighborIt == m_cementAdjacency.end())
            {
                for (auto scanIt = m_cementAdjacency.begin(); scanIt != m_cementAdjacency.end(); ++scanIt)
                {
                    if (WeakSweetsEqual{}(scanIt->first, neighborHandle))
                    {
                        neighborIt = scanIt;
                        break;
                    }
                }
            }

            if (neighborIt != m_cementAdjacency.end())
            {
                eraseByOwner(neighborIt->second);
                if (neighborIt->second.empty())
                {
                    m_cementAdjacency.erase(neighborIt);
                }
            }
        }

        m_cementAdjacency.erase(it);
    }

    void SweetsContacts::RemoveCakeAdjacencyNode(const WeakSweetsHandle& handle)
    {
        if (auto cakeIt = m_cementCakeAdjacency.find(kStageCakeKey);
            cakeIt != m_cementCakeAdjacency.end())
        {
            auto& cakeSet = cakeIt->second;
            for (auto it = cakeSet.begin(); it != cakeSet.end();)
            {
                if (WeakSweetsEqual{}(*it, handle))
                {
                    it = cakeSet.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            if (cakeSet.empty())
            {
                m_cementCakeAdjacency.erase(cakeIt);
            }
        }

        if (m_cementCakeAdjacency.empty())
        {
            m_cementCakeEntity = nullptr;
        }
    }
}
