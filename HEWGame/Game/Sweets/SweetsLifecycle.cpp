#include "Sweets/SweetsLifecycle.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

#include <box2d/box2d.h>

#include "AABB.h"
#include "CollisionLayer.h"
#include "Core/UpdateScheduler.h"
#include "Debug.h"
#include "Entities/PhysicalEntity.h"
#include "Sweets/BaseSweets.h"
#include "Sweets/SweetsContacts.h"
#include "Sweets/SweetsRegistry.h"
#include "Sweets/SweetsSpawner.h"
#include "Systems/Draw/DrawSystem.h"
#include "Systems/Physics/PhysicsMachine.h"

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace
{
    using SweetsSpace::DeleteReason;
    using SweetsSpace::SweetsState;
    using SweetsSpace::UseOutcome;

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
    constexpr float kRestVelocityThresholdSq = 0.05f * 0.05f;
    constexpr float kPlatformWidth = 12.0f;
    constexpr float kHorizontalOutOfBoundsMargin = 3.5f;
    constexpr float kHorizontalBoundsEpsilon = 1e-4f;
    constexpr float kOutOfBoundsRespawnDelaySeconds = 4.0f;
    constexpr float kGrabRespawnDelaySeconds = 20.0f;

    float ComputeSpeedSq(const Entities::PhysicalEntity& entity)
    {
        const auto v = entity.GetLinearVelocity();
        return (v.x * v.x) + (v.y * v.y) + (v.z * v.z);
    }

    float ComputeFallbackHorizontalLimit(float margin)
    {
        const float sanitizedMargin = std::max(0.0f, margin);
        return (kPlatformWidth * 0.5f) + sanitizedMargin;
    }

    bool HasMeaningfulDifference(float previous, float current, float epsilon = kHorizontalBoundsEpsilon)
    {
        if (!std::isfinite(previous) || !std::isfinite(current))
        {
            return true;
        }

        return std::fabs(previous - current) > epsilon;
    }

    bool HasMeaningfulDifference(const float3& a, const float3& b, float epsilon = kHorizontalBoundsEpsilon)
    {
        return HasMeaningfulDifference(a.x, b.x, epsilon)
            || HasMeaningfulDifference(a.y, b.y, epsilon)
            || HasMeaningfulDifference(a.z, b.z, epsilon);
    }

    const char* DeleteReasonToString(DeleteReason reason)
    {
        switch (reason)
        {
        case DeleteReason::Hit:
            return "Hit";
        case DeleteReason::Miss:
            return "Miss";
        default:
            return "<unknown>";
        }
    }

    const char* UseOutcomeToString(UseOutcome outcome)
    {
        switch (outcome)
        {
        case UseOutcome::Hit:
            return "Hit";
        case UseOutcome::Miss:
            return "Miss";
        default:
            return "<unknown>";
        }
    }

    const char* StateToString(SweetsState state)
    {
        switch (state)
        {
        case SweetsState::Active:
            return "Active";
        case SweetsState::Stacked:
            return "Stacked";
        case SweetsState::Ground:
            return "Ground";
        case SweetsState::Carried:
            return "Carried";
        default:
            return "Unknown";
        }
    }

    void UpdateBatchUsage(SweetsSpace::DeleteBatchContext* batch, SweetsSpace::UseOutcome outcome)
    {
        if (!batch)
        {
            return;
        }

        if (batch->usesRemaining > 0)
        {
            --batch->usesRemaining;
        }

        if (outcome == SweetsSpace::UseOutcome::Hit)
        {
            ++batch->usesRemaining;
        }
    }

    SweetsSpace::SweetsState EvaluateState(SweetsSpace::ActiveSweets& sweets,
        SweetsSpace::SweetsState currentState,
        const SweetsSpace::SweetsContacts& contacts)
    {
        if (currentState == SweetsSpace::SweetsState::Carried)
        {
            return SweetsSpace::SweetsState::Carried;
        }

        const float speedSq = ComputeSpeedSq(sweets);
        if (speedSq > kRestVelocityThresholdSq)
        {
            return SweetsSpace::SweetsState::Active;
        }

        bool foundationOnly = false;
        bool touching = contacts.HasSupportingContact(sweets, &foundationOnly);
        if (!touching)
        {
            if (auto* physicsMachine = sweets.GetPhysicsMachine())
            {
                const b2BodyId bodyId = physicsMachine->FindBody(sweets);
                if (b2Body_IsValid(bodyId))
                {
                    const int capacity = b2Body_GetContactCapacity(bodyId);
                    if (capacity > 0)
                    {
                        std::vector<b2ContactData> contactsBuffer(static_cast<std::size_t>(capacity));
                        const int count = b2Body_GetContactData(bodyId, contactsBuffer.data(), capacity);
                        for (int i = 0; i < count; ++i)
                        {
                            if (contactsBuffer[i].manifold.pointCount > 0)
                            {
                                touching = true;
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (touching)
        {
            if (foundationOnly)
            {
                return SweetsSpace::SweetsState::Active;
            }

            return currentState == SweetsSpace::SweetsState::Stacked
                ? SweetsSpace::SweetsState::Stacked
                : SweetsSpace::SweetsState::Active;
        }

        return SweetsSpace::SweetsState::Active;
    }

    bool IsOutOfBounds(
        ::SweetsSpace::ActiveSweets& sweets,
        const std::optional<std::pair<float, float>>& worldBounds,
        const std::optional<float>& minVerticalBound,
        float margin,
        float fallbackLimit,
        bool* usedFallback)
    {
        sweets.SyncColliderBounds();
        const AABB bounds = sweets.GetAABB();
        float minX = bounds.min.x;
        float maxX = bounds.max.x;

        const float sanitizedMargin = std::max(0.0f, margin);
        float allowedMin = -fallbackLimit;
        float allowedMax = fallbackLimit;
        if (usedFallback)
        {
            *usedFallback = !worldBounds.has_value();
        }

        if (worldBounds)
        {
            allowedMin = worldBounds->first - sanitizedMargin;
            allowedMax = worldBounds->second + sanitizedMargin;
        }

        const bool outOfHorizontalBounds = minX < allowedMin || maxX > allowedMax;
        const bool outOfVerticalBounds = minVerticalBound
            ? (bounds.max.y < *minVerticalBound)
            : false;

        return outOfHorizontalBounds || outOfVerticalBounds;
    }
}

namespace SweetsSpace
{
    SweetsLifecycle::SweetsLifecycle(SweetsRegistry& registry,
        SweetsContacts& contacts,
        SweetsSpawner& spawner,
        Systems::DrawSystem* drawSystem,
        Systems::PhysicsMachine* physicsMachine,
        Core::UpdateScheduler* scheduler,
        bool* warnedMissingDrawSystem,
        bool* warnedMissingPhysicsMachine,
        bool* warnedMissingScheduler)
        : m_registry(&registry)
        , m_contacts(&contacts)
        , m_spawner(&spawner)
        , m_drawSystem(drawSystem)
        , m_physicsMachine(physicsMachine)
        , m_scheduler(scheduler)
        , m_warnedMissingDrawSystem(warnedMissingDrawSystem)
        , m_warnedMissingPhysicsMachine(warnedMissingPhysicsMachine)
        , m_warnedMissingScheduler(warnedMissingScheduler)
        , m_outOfBoundsDespawnEnabled(true)
        , m_outOfBoundsRespawnEnabled(true)
        , m_outOfBoundsRespawnDelaySeconds(kOutOfBoundsRespawnDelaySeconds)
        , m_grabRespawnDelaySeconds(kGrabRespawnDelaySeconds)
        , m_horizontalBoundsMargin(kHorizontalOutOfBoundsMargin)
    {
    }

    void SweetsLifecycle::OnUpdate(const Core::UpdateContext& ctx)
    {
        struct PendingStateChange
        {
            std::shared_ptr<ActiveSweets> sweets;
            SweetsState state;
        };

        struct PendingRemoval
        {
            std::shared_ptr<ActiveSweets> sweets;
            bool notifySpawner;
        };

        std::vector<PendingStateChange> stateChanges;
        std::vector<PendingRemoval> missedSweets;
        const float deltaSeconds = std::max(0.0f, ctx.dt);
        const auto horizontalBounds = GetHorizontalBounds();
        const std::optional<float> verticalBound = m_hasVerticalBound
            ? std::optional<float>(m_minVerticalBound)
            : std::nullopt;
        const float fallbackHorizontalLimit = ComputeFallbackHorizontalLimit(m_horizontalBoundsMargin);

        if (!m_registry)
        {
            return;
        }

        for (auto& entry : m_registry->MutableStateMap())
        {
            SweetsState bucketState = entry.first;
            auto& sweetsList = entry.second;

            auto it = sweetsList.begin();
            while (it != sweetsList.end())
            {
                std::shared_ptr<ActiveSweets> sweets = *it;

                if (!sweets)
                {
#if defined(_DEBUG)
                    std::ostringstream oss;
                    oss << kLogPrefix << "状態 " << StateToString(bucketState)
                        << " から null のスイーツを掃除したよ\n";
                    DebugTrace(oss.str());
#endif
                    it = sweetsList.erase(it);
                    continue;
                }

                if (sweets->IsPendingDestroy())
                {
                    m_outOfBoundsTimers.erase(sweets);
                    m_grabRespawnTimers.erase(sweets);
                    ++it;
                    continue;
                }

                if (!sweets->IsEnabled())
                {
#if defined(_DEBUG)
                    std::ostringstream oss;
                    oss << kLogPrefix << "Update 無効化中だったから、状態 " << StateToString(bucketState)
                        << " から sweets ptr=" << sweets.get() << " を片付けたよ \n";
                    DebugTrace(oss.str());
#endif
                    m_outOfBoundsTimers.erase(sweets);
                    m_grabRespawnTimers.erase(sweets);
                    m_registry->Lookup().erase(sweets);
                    it = sweetsList.erase(it);
                    continue;
                }

                const bool isCarried = (bucketState == SweetsState::Carried);

                if (isCarried)
                {
                    auto timerIt = m_grabRespawnTimers.find(sweets);
                    if (timerIt != m_grabRespawnTimers.end())
                    {
                        timerIt->second += deltaSeconds;
                        if (timerIt->second >= m_grabRespawnDelaySeconds)
                        {
                            const bool requested = (m_spawner != nullptr)
                                ? m_spawner->RequestSupplementalSpawn(1, "GrabRespawn")
                                : false;
                            if (requested)
                            {
                                m_grabRespawnTimers.erase(timerIt);
                            }
                        }
                    }
                    m_outOfBoundsTimers.erase(sweets);
                    ++it;
                    continue;
                }

                m_grabRespawnTimers.erase(sweets);

                {
                    bool usedFallbackBounds = false;
                    const bool outOfBounds = IsOutOfBounds(
                        *sweets,
                        horizontalBounds,
                        verticalBound,
                        m_horizontalBoundsMargin,
                        fallbackHorizontalLimit,
                        &usedFallbackBounds);
                    if (usedFallbackBounds && !m_warnedMissingHorizontalBounds)
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "水平境界のフォールバックを使うね 幅制限="
                            << fallbackHorizontalLimit << " (半幅＋マージン) \n";
                        DebugTrace(oss.str());
                        m_warnedMissingHorizontalBounds = true;
                    }

                    if (outOfBounds && m_outOfBoundsDespawnEnabled)
                    {
                        float& timer = m_outOfBoundsTimers[sweets];
                        timer += deltaSeconds;
                        if (timer >= m_outOfBoundsRespawnDelaySeconds)
                        {
                            m_outOfBoundsTimers.erase(sweets);
                            missedSweets.push_back({ sweets, m_outOfBoundsRespawnEnabled });
                        }
                        ++it;
                        continue;
                    }

                    if (m_outOfBoundsTimers.erase(sweets) > 0)
                    {
#if defined(_DEBUG)
                        std::ostringstream oss;
                        oss << kLogPrefix << "画面外タイマーを解除 tag='" << sweets->GetTag() << "'\n";
                        DebugTrace(oss.str());
#endif
                    }

                    const SweetsState desiredState = EvaluateState(*sweets, bucketState, *m_contacts);
                    if (desiredState != bucketState)
                    {
                        stateChanges.push_back({ sweets, desiredState });
                    }
                }

                ++it;
            }
        }

        for (const auto& change : stateChanges)
        {
            auto lookup = m_registry->Lookup().find(change.sweets);
            const SweetsState previousState = (lookup != m_registry->Lookup().end()) ? lookup->second : SweetsState::Active;
            m_registry->SetState(change.sweets, change.state);
            if (m_contacts)
            {
                m_contacts->OnStateChanged(change.sweets.get(), previousState, change.state);
            }
            if (m_spawner)
            {
                m_spawner->OnStateChanged(change.sweets, previousState, change.state);
            }
#if defined(_DEBUG)
            std::ostringstream oss;
            oss << kLogPrefix << "状態変更 ptr=" << change.sweets.get()
                << " " << StateToString(previousState) << " -> "
                << StateToString(change.state) << "\n";
            DebugTrace(oss.str());
#endif
        }

        const int respawnCount = static_cast<int>(std::count_if(
            missedSweets.begin(),
            missedSweets.end(),
            [](const PendingRemoval& entry)
            {
                return entry.notifySpawner;
            }));
        if (respawnCount > 0 && m_spawner)
        {
            m_spawner->RequestSupplementalSpawn(respawnCount, "OutOfBoundsRespawn");
        }

        DeleteBatchContext missBatch{};
        missBatch.usesRemaining = static_cast<int>(missedSweets.size());
        for (const auto& entry : missedSweets)
        {
            RemoveSweetsEntry(entry.sweets, DeleteReason::Miss, "MissDespawn", true, &missBatch, entry.notifySpawner);
        }
    }

    void SweetsLifecycle::OnLateUpdate(const Core::UpdateContext& ctx)
    {
        (void)ctx;

        if (!m_registry)
        {
            return;
        }

        std::size_t removed = 0;
        std::size_t markedSafe = 0;
        std::size_t stagedThisFrame = 0;

        for (auto& entry : m_registry->MutableStateMap())
        {
            auto& sweetsList = entry.second;
            auto it = sweetsList.begin();
            while (it != sweetsList.end())
            {
                const auto& sweets = *it;
                if (!sweets)
                {
                    it = sweetsList.erase(it);
                    ++removed;
                    continue;
                }

                if (sweets->IsPendingDestroy())
                {
                    m_outOfBoundsTimers.erase(sweets);
                    m_grabRespawnTimers.erase(sweets);
                    auto watched = m_pendingLateDeletion.find(sweets);
                    if (watched == m_pendingLateDeletion.end())
                    {
                        m_pendingLateDeletion.insert(sweets);
                        ++stagedThisFrame;
                        ++it;
                        continue;
                    }

                    if (!sweets->IsSafeToDelete())
                    {
                        sweets->MarkSafeToDelete();
                        ++markedSafe;
                        ++it;
                        continue;
                    }

                    m_registry->Lookup().erase(sweets);
                    m_pendingLateDeletion.erase(watched);
                    m_outOfBoundsTimers.erase(sweets);
                    m_grabRespawnTimers.erase(sweets);
                    it = sweetsList.erase(it);
                    ++removed;
                    continue;
                }

                ++it;
            }
        }

#if defined(_DEBUG)
        if (removed > 0 || markedSafe > 0 || stagedThisFrame > 0)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "後処理で " << stagedThisFrame << " 件を待機、 " << markedSafe
                << " 件を削除OKにして、 " << removed
                << " 件のスイーツを消したよ \n";
            DebugTrace(oss.str());
        }
#endif
    }

    void SweetsLifecycle::SetOutOfBoundsSettings(bool despawnEnabled, bool respawnEnabled, float respawnDelaySeconds)
    {
        m_outOfBoundsDespawnEnabled = despawnEnabled;
        m_outOfBoundsRespawnEnabled = respawnEnabled;
        if (std::isfinite(respawnDelaySeconds))
        {
            m_outOfBoundsRespawnDelaySeconds = std::max(0.0f, respawnDelaySeconds);
        }
    }

    void SweetsLifecycle::SetGrabRespawnDelaySeconds(float respawnDelaySeconds)
    {
        if (std::isfinite(respawnDelaySeconds))
        {
            m_grabRespawnDelaySeconds = std::max(0.0f, respawnDelaySeconds);
        }
    }

    void SweetsLifecycle::SetHorizontalBounds(float minX, float maxX, std::optional<float> margin)
    {
        if (!(std::isfinite(minX) && std::isfinite(maxX)))
        {
            DebugTrace(std::string(kLogPrefix) + "有限値じゃない水平境界の更新は無視したよ \n");
            return;
        }

        const float sanitizedMin = std::min(minX, maxX);
        const float sanitizedMax = std::max(minX, maxX);
        const float sanitizedMargin = margin.has_value()
            ? std::max(0.0f, *margin)
            : m_horizontalBoundsMargin;

        const bool changed = !m_hasHorizontalBounds
            || HasMeaningfulDifference(m_minHorizontalBound, sanitizedMin)
            || HasMeaningfulDifference(m_maxHorizontalBound, sanitizedMax)
            || HasMeaningfulDifference(m_horizontalBoundsMargin, sanitizedMargin);

        m_hasHorizontalBounds = true;
        m_minHorizontalBound = sanitizedMin;
        m_maxHorizontalBound = sanitizedMax;
        m_horizontalBoundsMargin = sanitizedMargin;
        m_warnedMissingHorizontalBounds = false;

        if (!changed)
        {
            return;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "水平境界を更新したよ min=" << m_minHorizontalBound
            << " max=" << m_maxHorizontalBound
            << " margin=" << m_horizontalBoundsMargin << " (world units).\n";
        DebugTrace(oss.str());
    }

    void SweetsLifecycle::SetWorldBounds(float minX, float maxX, float minY, std::optional<float> margin)
    {
        SetHorizontalBounds(minX, maxX, margin);

        if (!std::isfinite(minY))
        {
            DebugTrace(std::string(kLogPrefix) + "有限値じゃない垂直境界の更新は無視したよ \n");
            return;
        }

        const bool changed = !m_hasVerticalBound
            || HasMeaningfulDifference(m_minVerticalBound, minY);

        m_hasVerticalBound = true;
        m_minVerticalBound = minY;

        if (!changed)
        {
            return;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "垂直境界を更新したよ minY=" << m_minVerticalBound << " (world units).\n";
        DebugTrace(oss.str());
    }

    std::optional<std::pair<float, float>> SweetsLifecycle::GetHorizontalBounds() const
    {
        if (!m_hasHorizontalBounds)
        {
            return std::nullopt;
        }

        return std::make_pair(m_minHorizontalBound, m_maxHorizontalBound);
    }

    void SweetsLifecycle::Erase(const std::shared_ptr<ActiveSweets>& sweets)
    {
        if (!sweets || !m_registry)
        {
            return;
        }

        auto found = m_registry->Lookup().find(sweets);
        if (found == m_registry->Lookup().end())
        {
            return;
        }

        if (sweets->IsPendingDestroy())
        {
            return;
        }

        sweets->MarkPendingDestroy();
    }

    void SweetsLifecycle::ClearAllSweets()
    {
        if (!m_registry)
        {
            return;
        }

        if (m_spawner)
        {
            m_spawner->ForceSpawnRefresh();
        }

        if (m_contacts)
        {
            m_contacts->Reset();
            m_contacts->ClearCreamWeldJoints("ClearAllSweets");
        }

        std::size_t staged = 0;
        std::size_t alreadyPending = 0;

        m_outOfBoundsTimers.clear();
        m_grabRespawnTimers.clear();

        for (auto& entry : m_registry->MutableStateMap())
        {
            auto& list = entry.second;
            for (auto& sweets : list)
            {
                if (!sweets)
                {
                    continue;
                }

                if (sweets->IsPendingDestroy())
                {
                    ++alreadyPending;
                }
                else
                {
                    sweets->MarkPendingDestroy();
                    sweets->Enable(false);
                    ++staged;
                }

                UnregisterSweetsFromSystems(sweets, "ClearAllSweets", true);
            }
        }

        std::ostringstream oss;
        oss << kLogPrefix << "ClearAllSweets で " << staged
            << " 個を破棄予約したよ（すでに予約済み " << alreadyPending << " 個）/ "
            << m_registry->StateMap().size() << " 状態分 \n";
        DebugTrace(oss.str());
    }

    bool SweetsLifecycle::TryMarkAsCarried(const std::shared_ptr<ActiveSweets>& sweets)
    {
        if (!sweets || !m_registry)
        {
            return false;
        }

        if (sweets->IsPendingDestroy())
        {
            return false;
        }

        sweets->SetWasEverCarried(true);
        if (m_contacts)
        {
            m_contacts->ClearContactState(sweets.get());
            m_contacts->RemoveCreamWeldJointsForSweets(sweets.get(), "TryMarkAsCarried");
        }

        if (m_physicsMachine)
        {
            m_physicsMachine->UnregisterBody(*sweets, true);
        }
        else if (m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
        {
            DebugTrace(std::string(kLogPrefix) + "TryMarkAsCarried で物理マシンが見つからないから、このままシミュレーションしちゃうかも \n");
            *m_warnedMissingPhysicsMachine = true;
        }

        auto previous = m_registry->SetState(sweets, SweetsState::Carried);
        if (previous && m_contacts)
        {
            m_contacts->OnStateChanged(sweets.get(), *previous, SweetsState::Carried);
        }

        if (m_grabRespawnTimers.find(sweets) == m_grabRespawnTimers.end())
        {
            m_grabRespawnTimers.emplace(sweets, 0.0f);
        }

        std::ostringstream oss;
        oss << kLogPrefix << "TryMarkAsCarried 成功 tag='" << sweets->GetTag() << "'\n";
        DebugTrace(oss.str());
        return true;
    }

    bool SweetsLifecycle::ReleaseCarriedSweets(const std::shared_ptr<ActiveSweets>& sweets)
    {
        if (!sweets || !m_registry)
        {
            return false;
        }

        if (sweets->IsPendingDestroy())
        {
            return false;
        }

        auto it = m_registry->Lookup().find(sweets);
        if (it == m_registry->Lookup().end() || it->second != SweetsState::Carried)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "ReleaseCarriedSweets 却下 tag='" << sweets->GetTag() << "' state="
                << (it == m_registry->Lookup().end() ? "<unknown>" : StateToString(it->second)) << "\n";
            DebugTrace(oss.str());
            return false;
        }

        auto previous = m_registry->SetState(sweets, SweetsState::Active);
        if (previous && m_contacts)
        {
            m_contacts->OnStateChanged(sweets.get(), *previous, SweetsState::Active);
        }
        if (m_contacts)
        {
            m_contacts->RemoveCreamWeldJointsForSweets(sweets.get(), "ReleaseCarriedSweets");
        }

        m_grabRespawnTimers.erase(sweets);

        if (m_physicsMachine)
        {
            m_physicsMachine->RegisterBody(*sweets);
        }
        else if (m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
        {
            DebugTrace(std::string(kLogPrefix) + "ReleaseCarriedSweets で物理マシンがないから、ボディがシミュレーションされないよ \n");
            *m_warnedMissingPhysicsMachine = true;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "ReleaseCarriedSweets 成功 tag='" << sweets->GetTag() << "'\n";
        DebugTrace(oss.str());
        return true;
    }

    bool SweetsLifecycle::RegisterWithSystems(const std::shared_ptr<ActiveSweets>& sweets)
    {
        if (!sweets)
        {
            return false;
        }

        const auto cleanupFailedRegistration = [&](const std::string& reason)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[Error] " << reason << '\n';
            DebugTrace(oss.str());

            if (m_contacts)
            {
                m_contacts->ClearContactState(sweets.get());
            }
            sweets->MarkPendingDestroy();
            sweets->Enable(false);
            UnregisterSweetsFromSystems(sweets, "RegisterWithSystems", false);

            if (m_registry)
            {
                for (auto& entry : m_registry->MutableStateMap())
                {
                    entry.second.remove(sweets);
                }
                m_registry->Lookup().erase(sweets);
            }
            return false;
        };

        if (m_scheduler)
        {
            auto handle = m_scheduler->Register(*sweets);
            if (!handle.IsValid())
            {
                DebugLog::DebugMessage("[SweetsLifecycle] Scheduler registration failed for sweets.\n");
                assert(false && "Sweets scheduler registration failed.");
                return cleanupFailedRegistration("スケジューラ登録に失敗したよ");
            }
            sweets->SetRegistrationHandle(std::move(handle));
        }
        else if (m_warnedMissingScheduler && !*m_warnedMissingScheduler)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "スイーツ '" << sweets->GetTag()
                << "' をスケジューラに登録できなかったよ スケジューラが設定されてないみたい \n";
            DebugTrace(oss.str());
            *m_warnedMissingScheduler = true;
        }

        if (m_physicsMachine)
        {
            const auto& colliderComponent = sweets->GetBoxColliderComponent();
            const bool hasColliderComponent = static_cast<bool>(colliderComponent);

            if (!hasColliderComponent)
            {
                std::ostringstream oss;
                oss << "スイーツ '" << sweets->GetTag() << "' の物理登録を飛ばすね コライダー設定が見当たらない";
                return cleanupFailedRegistration(oss.str());
            }

            const bool alreadyRegistered = m_registeredPhysicsBodies.find(sweets.get()) != m_registeredPhysicsBodies.end()
                || b2Body_IsValid(m_physicsMachine->FindBody(*sweets));
            if (!alreadyRegistered)
            {
                m_physicsMachine->RegisterBody(*sweets);
            }

            const int fixtureCount = colliderComponent ? colliderComponent->createdFixtureCount : 0;
            const bool hasBody = sweets->HasBox2DBody();

            if (!hasBody || fixtureCount <= 0)
            {
                std::ostringstream oss;
                oss << "スイーツ '" << sweets->GetTag() << "' の物理登録に失敗 hasBody=" << std::boolalpha << hasBody
                    << " fixtures=" << fixtureCount;
                return cleanupFailedRegistration(oss.str());
            }

            m_registeredPhysicsBodies.insert(sweets.get());
        }
        else if (m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "スイーツ '" << sweets->GetTag()
                << "' を物理に登録できなかったよ 物理マシンが設定されてない \n";
            DebugTrace(oss.str());
            *m_warnedMissingPhysicsMachine = true;
        }

        if (m_drawSystem)
        {
            m_drawSystem->RegisterRenderable(*sweets);
        }
        else if (m_warnedMissingDrawSystem && !*m_warnedMissingDrawSystem)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "スイーツ '" << sweets->GetTag()
                << "' を描画登録できなかったよ ドローシステムが設定されてない \n";
            DebugTrace(oss.str());
            *m_warnedMissingDrawSystem = true;
        }

        return true;
    }

    void SweetsLifecycle::RemoveSweetsEntry(const std::shared_ptr<ActiveSweets>& sweets,
        DeleteReason reason,
        const char* context,
        bool logWarnings,
        DeleteBatchContext* batchContext,
        bool notifySpawner)
    {
        if (!sweets || !m_registry)
        {
            return;
        }

        auto found = m_registry->Lookup().find(sweets);
        if (found == m_registry->Lookup().end())
        {
            return;
        }

        if (sweets->IsPendingDestroy())
        {
            return;
        }

        m_outOfBoundsTimers.erase(sweets);
        m_grabRespawnTimers.erase(sweets);

        const SweetsState previousState = found->second;
        if (m_contacts)
        {
            m_contacts->ClearContactState(sweets.get());
            m_contacts->ClearSweetsContact(sweets.get());
            m_contacts->RemoveCreamWeldJointsForSweets(sweets.get(), context);
        }
        sweets->SetWasEverCarried(false);
        sweets->MarkPendingDestroy();
        sweets->Enable(false);

        const UseOutcome outcome = (reason == DeleteReason::Hit) ? UseOutcome::Hit : UseOutcome::Miss;
        UpdateBatchUsage(batchContext, outcome);

        UnregisterSweetsFromSystems(sweets, context, logWarnings);

        if (m_spawner && notifySpawner)
        {
            const int remainingToStack = std::max(0, m_spawner->GetBatchStackTarget() - m_spawner->GetStackedInBatch());
            const int liveAfterRemoval = m_spawner->CountLiveSweets();
            m_spawner->OnSweetsRemoved(sweets, remainingToStack, liveAfterRemoval, context);
        }

#if defined(_DEBUG)
        const auto& pos = sweets->GetTransform().position;
        std::ostringstream oss;
        oss << kLogPrefix << context << " でスイーツ tag='" << sweets->GetTag()
            << "' を削除 state=" << StateToString(previousState)
            << " deleteReason=" << DeleteReasonToString(reason)
            << " outcome=" << UseOutcomeToString(outcome)
            << " batchUsesRemaining=" << (batchContext ? batchContext->usesRemaining : -1)
            << " pos=(" << pos.x << ',' << pos.y << ',' << pos.z << ")\n";
        DebugTrace(oss.str());
#endif
    }

    void SweetsLifecycle::UnregisterSweetsFromSystems(const std::shared_ptr<ActiveSweets>& sweets, const char* context, bool logWarnings)
    {
        if (!sweets)
        {
            return;
        }

        if (m_contacts)
        {
            m_contacts->RemoveCreamWeldJointsForSweets(sweets.get(), context);
        }

        if (m_scheduler)
        {
            sweets->ReleaseRegistrationHandle();
        }
        else if (logWarnings && m_warnedMissingScheduler && !*m_warnedMissingScheduler)
        {
            std::ostringstream oss;
            oss << kLogPrefix << context << " でスイーツ '" << sweets->GetTag()
                << "' をスケジューラから外せなかったよ スケジューラ未設定 \n";
            DebugTrace(oss.str());
            *m_warnedMissingScheduler = true;
        }

        if (m_physicsMachine)
        {
            const bool tracked = m_registeredPhysicsBodies.erase(sweets.get()) > 0;
            if (tracked || b2Body_IsValid(m_physicsMachine->FindBody(*sweets)))
            {
                m_physicsMachine->UnregisterBody(*sweets);
            }
        }
        else if (logWarnings && m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
        {
            std::ostringstream oss;
            oss << kLogPrefix << context << " でスイーツ '" << sweets->GetTag()
                << "' の物理登録を外せなかったよ 物理マシン未設定 \n";
            DebugTrace(oss.str());
            *m_warnedMissingPhysicsMachine = true;
        }

        if (m_drawSystem)
        {
            m_drawSystem->UnregisterRenderable(*sweets);
        }
        else if (logWarnings && m_warnedMissingDrawSystem && !*m_warnedMissingDrawSystem)
        {
            std::ostringstream oss;
            oss << kLogPrefix << context << " でスイーツ '" << sweets->GetTag()
                << "' の描画登録を外せなかったよ ドローシステム未設定 \n";
            DebugTrace(oss.str());
            *m_warnedMissingDrawSystem = true;
        }
    }
}
