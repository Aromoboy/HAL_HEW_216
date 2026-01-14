#include "Sweets/SweetsContacts.h"

#include <algorithm>
#include <queue>
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
    constexpr float kCreamWeldLinearHertz = 6.0f;
    constexpr float kCreamWeldAngularHertz = 6.0f;
    constexpr float kCreamWeldLinearDampingRatio = 0.7f;
    constexpr float kCreamWeldAngularDampingRatio = 0.7f;
    constexpr float kCreamWeldBreakGraceSeconds = 0.25f;
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

    void SweetsContacts::ConfigureCreamAdhesion(const SceneGameConfig::CreamAdhesionSettings& settings)
    {
        m_creamAdhesionDelaySeconds = std::max(0.0f, settings.adhesionDelaySeconds);
        m_creamAdhesionGraceSeconds = std::max(0.0f, settings.adhesionGraceSeconds);
        m_creamRequireSupport = settings.requireSupport;
        m_creamRequireCarried = settings.requireCarried;
        m_creamAllowAirAdhesionToSweets = settings.allowAirAdhesionToSweets;
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

        UpdateCementAdjacencyBegin(a, b);

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
            if (auto breakIt = m_pendingBreaks.find(key); breakIt != m_pendingBreaks.end())
            {
                m_pendingBreaks.erase(breakIt);
            }
            if (m_creamRequireCarried && !cream->WasEverCarried())
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
                    }
                    else
                    {
                        pendingIt->second.cream = cream;
                        pendingIt->second.other = other;
                        pendingIt->second.stillTouching = true;
                        pendingIt->second.awaitingRestore = false;
                        pendingIt->second.graceSecondsRemaining = 0.0f;
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

        UpdateCementAdjacencyEnd(a, b);

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

    void SweetsContacts::OnUpdate(float dt)
    {
        CleanupInvalidContacts();

        if (m_pendingAdhesions.empty() && m_pendingBreaks.empty())
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
            if (IsPendingDeletion(pending.cream) || IsPendingDeletion(pending.other))
            {
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

            if ((m_creamRequireCarried && !pending.cream->WasEverCarried())
                || !IsAdhesionTargetAllowed(*pending.cream, *pending.other))
            {
                it = m_pendingAdhesions.erase(it);
                continue;
            }

            TryCreateCreamWeldJoint(pending.cream, pending.other);
            it = m_pendingAdhesions.erase(it);
        }

        for (auto it = m_pendingBreaks.begin(); it != m_pendingBreaks.end();)
        {
            PendingBreak& pending = it->second;
            if (IsPendingDeletion(pending.cream) || IsPendingDeletion(pending.other))
            {
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
        ClearCementClusters("Reset");
        m_pendingAdhesions.clear();
        m_pendingBreaks.clear();
    }

    void SweetsContacts::RemoveCreamWeldJointsForSweets(ActiveSweets* sweets, const char* context)
    {
        if (!sweets)
        {
            return;
        }

        ClearPendingAdhesionsForSweets(sweets);
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

    void SweetsContacts::CleanupInvalidContacts()
    {
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
    }

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
            logRefusedAdhesion("物理マシン未設定");
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
            logRefusedAdhesion("空中で支えがない");
            return;
        }

        const ContactState* creamContact = FindContactState(*cream);
        const bool creamHasFoundationOrStage = creamContact
            && (creamContact->foundationContacts > 0 || creamContact->stageContacts > 0);
        if (m_creamRequireSupport && !creamHasFoundationOrStage && !creamLocked && !otherLocked
            && !otherSupportedForAdhesion && !allowAirAdhesion)
        {
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

        BuildCementCluster(cream, other);
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

    void SweetsContacts::UpdateCementAdjacencyBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b)
    {
        if (!a || !b)
        {
            return;
        }

        if (IsPendingDeletion(a) || IsPendingDeletion(b))
        {
            return;
        }

        if (!IsCementClusterCandidate(*a) || !IsCementClusterCandidate(*b))
        {
            return;
        }

        m_cementAdjacency[a].insert(b);
        m_cementAdjacency[b].insert(a);
    }

    void SweetsContacts::UpdateCementAdjacencyEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b)
    {
        if (!a || !b)
        {
            return;
        }

        auto itA = m_cementAdjacency.find(a);
        if (itA != m_cementAdjacency.end())
        {
            itA->second.erase(b);
            if (itA->second.empty())
            {
                m_cementAdjacency.erase(itA);
            }
        }

        auto itB = m_cementAdjacency.find(b);
        if (itB != m_cementAdjacency.end())
        {
            itB->second.erase(a);
            if (itB->second.empty())
            {
                m_cementAdjacency.erase(itB);
            }
        }
    }

    void SweetsContacts::RemoveCementAdjacencyForEntity(Entities::PhysicalEntity* entity)
    {
        if (!entity)
        {
            return;
        }

        auto it = m_cementAdjacency.find(entity);
        if (it == m_cementAdjacency.end())
        {
            return;
        }

        for (auto* neighbor : it->second)
        {
            auto neighborIt = m_cementAdjacency.find(neighbor);
            if (neighborIt != m_cementAdjacency.end())
            {
                neighborIt->second.erase(entity);
                if (neighborIt->second.empty())
                {
                    m_cementAdjacency.erase(neighborIt);
                }
            }
        }

        m_cementAdjacency.erase(it);
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

        std::unordered_set<Entities::PhysicalEntity*> visited;
        std::queue<Entities::PhysicalEntity*> frontier;
        visited.insert(cream);
        frontier.push(cream);
        if (other != cream)
        {
            visited.insert(other);
            frontier.push(other);
        }

        while (!frontier.empty())
        {
            auto* current = frontier.front();
            frontier.pop();
            auto adjIt = m_cementAdjacency.find(current);
            if (adjIt == m_cementAdjacency.end())
            {
                continue;
            }

            for (auto* neighbor : adjIt->second)
            {
                if (!neighbor || visited.find(neighbor) != visited.end())
                {
                    continue;
                }

                if (IsPendingDeletion(neighbor) || !IsCementClusterCandidate(*neighbor))
                {
                    continue;
                }

                visited.insert(neighbor);
                frontier.push(neighbor);
            }
        }

        auto cluster = std::make_shared<CementCluster>();
        cluster->members = visited;
        for (auto* member : visited)
        {
            if (auto* sweets = dynamic_cast<ActiveSweets*>(member))
            {
                cluster->sweets.insert(sweets);
            }

            if (!cluster->anchor && IsCakeEntity(*member))
            {
                cluster->anchor = member;
                cluster->anchoredToCake = true;
            }
        }

        if (!cluster->anchor)
        {
            if (cluster->sweets.find(cream) != cluster->sweets.end())
            {
                cluster->anchor = cream;
            }
            else if (!cluster->sweets.empty())
            {
                cluster->anchor = *cluster->sweets.begin();
            }
            else
            {
                cluster->anchor = other;
            }
        }

        if (!cluster->anchor)
        {
            return nullptr;
        }

        std::unordered_set<std::shared_ptr<CementCluster>> clustersToRemove;
        for (auto* sweets : cluster->sweets)
        {
            auto it = m_cementClusterLookup.find(sweets);
            if (it != m_cementClusterLookup.end())
            {
                clustersToRemove.insert(it->second);
            }
        }

        for (const auto& existing : clustersToRemove)
        {
            RemoveCementCluster(existing, "CementClusterRebuild");
        }

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
                kCreamWeldLinearHertz,
                kCreamWeldLinearDampingRatio,
                kCreamWeldAngularHertz,
                kCreamWeldAngularDampingRatio,
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
}
