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

    int GetBodyShapeCount(const Entities::PhysicalEntity* entity)
    {
        if (!entity)
        {
            return 0;
        }

        const b2BodyId bodyId = entity->GetBox2DBodyId();
        if (!b2Body_IsValid(bodyId))
        {
            return 0;
        }

        return b2Body_GetShapeCount(bodyId);
    }

    int GetBodyFixtureCount(const Entities::PhysicalEntity* entity)
    {
        if (!entity)
        {
            return 0;
        }

        const auto& colliderComponent = entity->GetBoxColliderComponent();
        return colliderComponent ? colliderComponent->createdFixtureCount : 0;
    }

    void AppendPendingEntityInfo(std::ostringstream& oss, const char* label, const Entities::PhysicalEntity* entity)
    {
        if (!label)
        {
            return;
        }

        oss << " " << label << "_tag='";
        if (entity)
        {
            oss << entity->GetTag() << "'";
        }
        else
        {
            oss << "<null>'";
        }
        oss << " " << label << "_shapeCount=" << GetBodyShapeCount(entity)
            << " " << label << "_fixtureCount=" << GetBodyFixtureCount(entity);
    }

    void LogPendingAdhesionRemoval(const char* reason,
        const Entities::PhysicalEntity* cream,
        const Entities::PhysicalEntity* other)
    {
        if (!cream && !other)
        {
            return;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "pending adhesion removed";
        if (reason && *reason)
        {
            oss << " reason=" << reason;
        }
        AppendPendingEntityInfo(oss, "cream", cream);
        AppendPendingEntityInfo(oss, "other", other);
        oss << "\n";
        LogInfoAndTrace(oss.str());
    }

    void LogInvalidPendingEntity(const char* pendingLabel, const Entities::PhysicalEntity* entity)
    {
        if (!pendingLabel || !entity)
        {
            return;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "pending " << pendingLabel << " removed (invalid entity) tag='"
            << entity->GetTag() << "' shapeCount=" << GetBodyShapeCount(entity)
            << " fixtureCount=" << GetBodyFixtureCount(entity) << "\n";
        LogInfoAndTrace(oss.str());
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

        const int shapeCount = b2Body_GetShapeCount(bodyId);
        if (shapeCount > 0 && GetBodyFixtureCount(entity) == 0)
        {
            return true;
        }

        if (HasValidBodyAndShapes(entity))
        {
            return true;
        }

        return shapeCount > 0;
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

    void SweetsContacts::TickCreamContacts()
    {
        if (m_isResetting || !m_physicsMachine || !m_registry)
        {
            return;
        }

        m_contactStates.clear();

        const auto& lookup = m_registry->Lookup();
        if (lookup.empty() && m_adhesionTargetLookup.empty())
        {
            return;
        }

        const std::size_t registryLookupCount = lookup.size();
        const int registryTotalCount = m_registry->CountAll();
        std::unordered_set<JointPairKey, JointPairHash> touchingPairs;
        touchingPairs.reserve(m_pendingAdhesions.size() + m_creamWeldJoints.size());
        std::unordered_set<JointPairKey, JointPairHash> processedContacts;
        processedContacts.reserve(m_pendingAdhesions.size() + m_creamWeldJoints.size());

        auto processContactPair = [&](ActiveSweets* sweets, Entities::PhysicalEntity* other)
        {
            if (!sweets || !other)
            {
                return;
            }

            const JointPairKey key = JointPairKey::Make(sweets, other);
            const bool contactInserted = processedContacts.insert(key).second;
            if (contactInserted)
            {
                UpdateContactStateBegin(sweets, other);
                if (auto* otherSweets = dynamic_cast<ActiveSweets*>(other))
                {
                    UpdateContactStateBegin(otherSweets, sweets);
                }
            }

            ActiveSweets* cream = nullptr;
            Entities::PhysicalEntity* creamOther = nullptr;
            if (IsCreamTag(sweets->GetTag()))
            {
                cream = sweets;
                creamOther = other;
            }
            else if (auto* otherSweets = dynamic_cast<ActiveSweets*>(other);
                otherSweets && IsCreamTag(otherSweets->GetTag()))
            {
                cream = otherSweets;
                creamOther = sweets;
            }

            if (!cream || !creamOther)
            {
                return;
            }

            const JointPairKey creamKey = JointPairKey::Make(cream, creamOther);
            touchingPairs.insert(creamKey);
            if (!contactInserted)
            {
                return;
            }

            if (auto breakIt = m_pendingBreaks.find(creamKey); breakIt != m_pendingBreaks.end())
            {
                m_pendingBreaks.erase(breakIt);
            }

            bool pendingCreated = false;
            float pendingElapsedSeconds = 0.0f;
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
            else if (IsAdhesionTargetAllowed(*cream, *creamOther))
            {
                auto existing = m_creamWeldJoints.find(creamKey);
                if (existing != m_creamWeldJoints.end() && !b2Joint_IsValid(existing->second))
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "壊れたクリーム接着ジョイントを掃除したよ cream='"
                        << cream->GetTag() << "' other='" << creamOther->GetTag() << "'\n";
                    DebugTrace(oss.str());
                    m_creamWeldJoints.erase(existing);
                    existing = m_creamWeldJoints.end();
                }

                if (existing == m_creamWeldJoints.end())
                {
                    auto pendingIt = m_pendingAdhesions.find(creamKey);
                    if (pendingIt == m_pendingAdhesions.end())
                    {
                        PendingAdhesion pending{};
                        pending.cream = cream;
                        pending.other = creamOther;
                        pending.elapsedSeconds = 0.0f;
                        pending.stillTouching = true;
                        pending.awaitingRestore = false;
                        pending.graceSecondsRemaining = 0.0f;
                        m_pendingAdhesions.emplace(creamKey, pending);
                        pendingCreated = true;
                    }
                    else
                    {
                        if (ShouldLogCreamPairEvent(cream, creamOther, "duplicateContactBegin"))
                        {
                            std::ostringstream oss;
                            oss << kLogPrefix << "duplicateContactBegin cream='" << cream->GetTag()
                                << "' other='" << creamOther->GetTag() << "'\n";
                            LogInfoAndTrace(oss.str());
                        }
                        pendingIt->second.cream = cream;
                        pendingIt->second.other = creamOther;
                        pendingIt->second.stillTouching = true;
                        pendingIt->second.awaitingRestore = false;
                        pendingIt->second.graceSecondsRemaining = 0.0f;
                        pendingElapsedSeconds = pendingIt->second.elapsedSeconds;
                    }
                }
            }

            if (auto pendingIt = m_pendingAdhesions.find(creamKey); pendingIt != m_pendingAdhesions.end())
            {
                pendingIt->second.stillTouching = true;
                pendingIt->second.awaitingRestore = false;
                pendingIt->second.graceSecondsRemaining = 0.0f;
                pendingElapsedSeconds = pendingIt->second.elapsedSeconds;
            }

            if (ShouldLogCreamPairEvent(cream, creamOther, "contact_begin"))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "cream contact begin cream='" << cream->GetTag()
                    << "' other='" << creamOther->GetTag()
                    << "' layer=" << static_cast<int>(creamOther->GetCollisionLayer())
                    << " elapsed=" << pendingElapsedSeconds
                    << " pending_created=" << std::boolalpha << pendingCreated << std::noboolalpha
                    << "\n";
                LogInfoAndTrace(oss.str());
            }
        };

        auto pollContactBody = [&](Entities::PhysicalEntity* entity, bool entityIsSweets)
        {
            if (!entity || IsPendingDeletion(entity))
            {
                return;
            }

            const b2BodyId entityBodyId = entity->GetBox2DBodyId();
            if (!b2Body_IsValid(entityBodyId))
            {
                return;
            }

            const int capacity = b2Body_GetContactCapacity(entityBodyId);
            if (capacity <= 0)
            {
                return;
            }

            std::vector<b2ContactData> contactsBuffer(static_cast<std::size_t>(capacity));
            const int count = b2Body_GetContactData(entityBodyId, contactsBuffer.data(), capacity);
            if (count <= 0)
            {
                return;
            }

            const uint64_t entityKey = b2StoreBodyId(entityBodyId);
            for (int i = 0; i < count; ++i)
            {
                const b2ContactData& contact = contactsBuffer[static_cast<std::size_t>(i)];
                if (contact.manifold.pointCount <= 0)
                {
                    continue;
                }

                if (!b2Shape_IsValid(contact.shapeIdA) || !b2Shape_IsValid(contact.shapeIdB))
                {
                    continue;
                }

                const b2BodyId bodyIdA = b2Shape_GetBody(contact.shapeIdA);
                const b2BodyId bodyIdB = b2Shape_GetBody(contact.shapeIdB);
                if (!b2Body_IsValid(bodyIdA) || !b2Body_IsValid(bodyIdB))
                {
                    continue;
                }

                const uint64_t bodyKeyA = b2StoreBodyId(bodyIdA);
                const uint64_t bodyKeyB = b2StoreBodyId(bodyIdB);
                b2BodyId otherBodyId = b2_nullBodyId;
                if (bodyKeyA == entityKey && bodyKeyB != entityKey)
                {
                    otherBodyId = bodyIdB;
                }
                else if (bodyKeyB == entityKey && bodyKeyA != entityKey)
                {
                    otherBodyId = bodyIdA;
                }
                else
                {
                    continue;
                }

                const auto resolved = m_physicsMachine->ResolveContactEntity(otherBodyId);
                if (!resolved || !resolved->entity)
                {
                    continue;
                }

                Entities::PhysicalEntity* other = resolved->entity;
                if (!other || other == entity || IsPendingDeletion(other))
                {
                    continue;
                }

                if (entityIsSweets)
                {
                    auto* sweets = static_cast<ActiveSweets*>(entity);
                    processContactPair(sweets, other);
                }
                else if (auto* otherSweets = dynamic_cast<ActiveSweets*>(other))
                {
                    processContactPair(otherSweets, entity);
                }
            }
        };

        for (const auto& entry : lookup)
        {
            ActiveSweets* sweets = entry.first.get();
            pollContactBody(sweets, true);
        }

        std::vector<Entities::PhysicalEntity*> adhesionTargets;
        if (!m_adhesionTargetLookup.empty())
        {
            adhesionTargets.reserve(m_adhesionTargetLookup.size());
            for (auto it = m_adhesionTargetLookup.begin(); it != m_adhesionTargetLookup.end();)
            {
                if (auto locked = it->second.lock())
                {
                    adhesionTargets.push_back(locked.get());
                    ++it;
                }
                else
                {
                    it = m_adhesionTargetLookup.erase(it);
                }
            }
        }

        for (auto* target : adhesionTargets)
        {
            pollContactBody(target, false);
        }

        {
            std::ostringstream oss;
            oss << kLogPrefix << "TickCreamContacts registry=" << registryLookupCount
                << " registryTotal=" << registryTotalCount
                << " processedContacts=" << processedContacts.size()
                << " adhesionTargets=" << adhesionTargets.size() << "\n";
            DebugTrace(oss.str());
        }

        for (auto& entry : m_pendingAdhesions)
        {
            PendingAdhesion& pending = entry.second;
            if (touchingPairs.find(entry.first) == touchingPairs.end() && pending.stillTouching)
            {
                pending.stillTouching = false;
                pending.awaitingRestore = true;
                pending.graceSecondsRemaining =
                    std::max(pending.graceSecondsRemaining, m_creamAdhesionGraceSeconds);
            }
        }

        for (const auto& entry : m_creamWeldJoints)
        {
            const JointPairKey& key = entry.first;
            if (touchingPairs.find(key) != touchingPairs.end())
            {
                continue;
            }

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

    void SweetsContacts::OnUpdate(float dt)
    {
        UpdateCreamAdhesionState(dt);
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

        const bool hasPendingRebuilds =
            !m_pendingCementClusterRebuilds.empty() || m_cementClusterRebuildDelayFrames > 0;
        if (m_pendingCementClusters.empty() && !hasPendingRebuilds)
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

            ClearPendingCementClustersForEntity(entity);
        };

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
        if (!entity)
        {
            return;
        }

        for (auto it = m_pendingAdhesions.begin(); it != m_pendingAdhesions.end();)
        {
            if (it->second.cream == entity || it->second.other == entity)
            {
                LogPendingAdhesionRemoval("clear_for_entity", it->second.cream, it->second.other);
                it = m_pendingAdhesions.erase(it);
            }
            else
            {
                ++it;
            }
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
    }

    void SweetsContacts::RequestCreamWeldBreak(ActiveSweets* cream, Entities::PhysicalEntity* other, const char* context)
    {
        if (!cream || !other)
        {
            return;
        }

        BreakCreamWeldJoint(cream, other, context ? context : "RequestCreamWeldBreak", CreamBondBreakReason::Manual);
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
        m_creamCandidates.clear();
        m_creamBonds.clear();
        m_creamAdhesionClockSeconds = 0.0f;
        m_creamAdhesionEventLogCount = 0;
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

        for (auto it = m_pendingAdhesions.begin(); it != m_pendingAdhesions.end();)
        {
            const bool creamValid = HasValidContactStateEntity(it->second.cream);
            const bool otherValid = HasValidContactStateEntity(it->second.other);
            if (!creamValid || !otherValid)
            {
                LogPendingAdhesionRemoval("invalid_contact_state", it->second.cream, it->second.other);
                if (!creamValid)
                {
                    appendInvalidEntity(it->second.cream);
                    LogInvalidPendingEntity("adhesion", it->second.cream);
                }
                if (!otherValid)
                {
                    appendInvalidEntity(it->second.other);
                    LogInvalidPendingEntity("adhesion", it->second.other);
                }
                it = m_pendingAdhesions.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (auto it = m_pendingCementClusters.begin(); it != m_pendingCementClusters.end();)
        {
            const bool creamValid = HasValidContactStateEntity(it->second.cream);
            const bool otherValid = HasValidContactStateEntity(it->second.other);
            if (!creamValid || !otherValid)
            {
                if (!creamValid)
                {
                    appendInvalidEntity(it->second.cream);
                    LogInvalidPendingEntity("cement", it->second.cream);
                }
                if (!otherValid)
                {
                    appendInvalidEntity(it->second.other);
                    LogInvalidPendingEntity("cement", it->second.other);
                }
                it = m_pendingCementClusters.erase(it);
            }
            else
            {
                ++it;
            }
        }

        for (Entities::PhysicalEntity* entity : invalidEntities)
        {
            for (auto it = m_pendingAdhesions.begin(); it != m_pendingAdhesions.end();)
            {
                if (it->second.cream == entity || it->second.other == entity)
                {
                    LogPendingAdhesionRemoval("invalid_entity", it->second.cream, it->second.other);
                    LogInvalidPendingEntity("adhesion", entity);
                    it = m_pendingAdhesions.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            for (auto it = m_pendingBreaks.begin(); it != m_pendingBreaks.end();)
            {
                if (it->second.cream == entity || it->second.other == entity)
                {
                    LogInvalidPendingEntity("break", entity);
                    it = m_pendingBreaks.erase(it);
                }
                else
                {
                    ++it;
                }
            }

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
        m_creamBonds.clear();
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
            LogCreamAdhesionEvent("contact_begin_reject_not_carried", cream, target);
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
        if (!hasContactPoint)
        {
            impulse = std::isfinite(approachSpeed) ? std::max(0.0f, approachSpeed) : 0.0f;
        }

        if (auto hitIt = m_contactHitImpulses.find(JointPairKey::Make(cream, target));
            hitIt != m_contactHitImpulses.end())
        {
            hitIt->second = std::max(hitIt->second, impulse);
        }
        else
        {
            m_contactHitImpulses.emplace(JointPairKey::Make(cream, target), impulse);
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
                || IsPendingDeletion(candidate.cream) || IsPendingDeletion(candidate.target))
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

            if (m_creamRequireCarried && !IsCreamCarried(*candidate.cream))
            {
                candidate.lastRejectReason = CreamAdhesionRejectReason::RequireCarried;
                it = m_creamCandidates.erase(it);
                continue;
            }

            if (!IsAdhesionTargetAllowed(*candidate.cream, *candidate.target))
            {
                candidate.lastRejectReason = CreamAdhesionRejectReason::TargetNotAllowed;
                it = m_creamCandidates.erase(it);
                continue;
            }

            CreamAdhesionPairKey key{ candidate.cream, candidate.target };
            auto bondIt = m_creamBonds.find(key);
            if (bondIt != m_creamBonds.end() && b2Joint_IsValid(bondIt->second.jointId))
            {
                candidate.lastRejectReason = CreamAdhesionRejectReason::ExistingBond;
                it = m_creamCandidates.erase(it);
                continue;
            }

            if (bondIt != m_creamBonds.end() && bondIt->second.cooldownRemaining > 0.0f)
            {
                candidate.lastRejectReason = CreamAdhesionRejectReason::CooldownActive;
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
            if (!bond.cream || !bond.target || IsPendingDeletion(bond.cream) || IsPendingDeletion(bond.target))
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

    bool SweetsContacts::TryCreateCreamWeldJoint(ActiveSweets* cream, Entities::PhysicalEntity* other, float touchingSeconds)
    {
        if (!cream || !other)
        {
            return false;
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

        if (!IsAdhesionTargetAllowed(*cream, *other))
        {
            if (ShouldLogCreamPairEvent(cream, other, "target_disallowed"))
            {
                std::ostringstream oss;
                oss << kLogPrefix << "cream weld early return reason=target disallowed cream='"
                    << cream->GetTag() << "' other='" << other->GetTag() << "'\n";
                LogInfoAndTrace(oss.str());
            }
            return false;
        }

        const bool creamLocked = IsCementedOrLocked(*cream);
        const bool otherLocked = IsCementedOrLocked(*other);
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

        std::ostringstream cementLog;
        cementLog << kLogPrefix << "cementで '" << cream->GetTag() << "' と '" << other->GetTag()
                  << "' を溶接したよ \n";
        LogInfoAndTrace(cementLog.str());

        if (auto pendingIt = m_pendingCementClusters.find(key); pendingIt != m_pendingCementClusters.end())
        {
            m_pendingCementClusters.erase(pendingIt);
        }
        LogInfoAndTrace(std::string(kLogPrefix) + "cement cluster disabled\n");
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
        else if (b2Joint_IsValid(jointId))
        {
            m_physicsMachine->DestroyJoint(jointId);
            std::ostringstream cementLog;
            cementLog << kLogPrefix << "cementの溶接を外したよ '" << cream->GetTag() << "' と '"
                      << other->GetTag() << "'\n";
            DebugTrace(cementLog.str());
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
