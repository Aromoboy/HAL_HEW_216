#include "Meteors/MeteorsLifecycle.h"

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
#include "Meteors/BaseMeteors.h"
#include "Meteors/MeteorsContacts.h"
#include "Meteors/MeteorsRegistry.h"
#include "Meteors/MeteorsSpawner.h"
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
    using MeteorsSpace::DeleteReason;
    using MeteorsSpace::MeteorsState;
    using MeteorsSpace::UseOutcome;

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

    constexpr const char* kLogPrefix = "[MeteorsManager] ";
    constexpr float kRestVelocityThresholdSq = 0.05f * 0.05f;
    constexpr float kPlatformWidth = 12.0f;
    constexpr float kHorizontalOutOfBoundsMargin = 3.5f;
    constexpr float kHorizontalBoundsEpsilon = 1e-4f;
    constexpr float kOutOfViewTimeoutSeconds = 25.0f;

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

    const char* StateToString(MeteorsState state)
    {
        switch (state)
        {
        case MeteorsState::Active:
            return "Active";
        case MeteorsState::Stacked:
            return "Stacked";
        case MeteorsState::Ground:
            return "Ground";
        case MeteorsState::Carried:
            return "Carried";
        default:
            return "Unknown";
        }
    }

    void UpdateBatchUsage(MeteorsSpace::DeleteBatchContext* batch, MeteorsSpace::UseOutcome outcome)
    {
        if (!batch)
        {
            return;
        }

        if (batch->usesRemaining > 0)
        {
            --batch->usesRemaining;
        }

        if (outcome == MeteorsSpace::UseOutcome::Hit)
        {
            ++batch->usesRemaining;
        }
    }

    MeteorsSpace::MeteorsState EvaluateState(MeteorsSpace::ActiveMeteors& meteors,
        MeteorsSpace::MeteorsState currentState,
        const MeteorsSpace::MeteorsContacts& contacts)
    {
        if (currentState == MeteorsSpace::MeteorsState::Carried)
        {
            return MeteorsSpace::MeteorsState::Carried;
        }

        const float speedSq = ComputeSpeedSq(meteors);
        if (speedSq > kRestVelocityThresholdSq)
        {
            return MeteorsSpace::MeteorsState::Active;
        }

        bool foundationOnly = false;
        bool touching = contacts.HasSupportingContact(meteors, &foundationOnly);
        if (!touching)
        {
            if (auto* physicsMachine = meteors.GetPhysicsMachine())
            {
                const b2BodyId bodyId = physicsMachine->FindBody(meteors);
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
                return MeteorsSpace::MeteorsState::Active;
            }

            return currentState == MeteorsSpace::MeteorsState::Stacked
                ? MeteorsSpace::MeteorsState::Stacked
                : MeteorsSpace::MeteorsState::Active;
        }

        return MeteorsSpace::MeteorsState::Active;
    }

    bool WrapMeteorsHorizontally(
        ::MeteorsSpace::ActiveMeteors& meteors,
        const std::optional<std::pair<float, float>>& worldBounds,
        float margin,
        float fallbackLimit,
        bool* usedFallback,
        bool* wasOutOfBounds)
    {
        meteors.SyncColliderBounds();
        const AABB bounds = meteors.GetAABB();
        const float minX = bounds.min.x;
        const float maxX = bounds.max.x;

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

        const bool outOfBounds = minX < allowedMin || maxX > allowedMax;
        if (wasOutOfBounds)
        {
            *wasOutOfBounds = outOfBounds;
        }

        if (!outOfBounds)
        {
            return false;
        }

        const float span = allowedMax - allowedMin;
        if (span <= kHorizontalBoundsEpsilon)
        {
            return false;
        }

        float3 position = meteors.GetTransform().position;
        if (minX < allowedMin)
        {
            position.x += span;
        }
        else if (maxX > allowedMax)
        {
            position.x -= span;
        }

        meteors.SetPositionViaPhysics(position);
        return true;
    }
}

namespace MeteorsSpace
{
    MeteorsLifecycle::MeteorsLifecycle(MeteorsRegistry& registry,
        MeteorsContacts& contacts,
        MeteorsSpawner& spawner,
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
        , m_horizontalBoundsMargin(kHorizontalOutOfBoundsMargin)
    {
    }

    void MeteorsLifecycle::OnUpdate(const Core::UpdateContext& ctx)
    {
        struct PendingStateChange
        {
            std::shared_ptr<ActiveMeteors> meteors;
            MeteorsState state;
        };

        const float deltaSeconds = std::max(0.0f, ctx.dt);
        std::vector<PendingStateChange> stateChanges;
        std::vector<std::shared_ptr<ActiveMeteors>> missedMeteors;
        const auto horizontalBounds = GetHorizontalBounds();
        const float fallbackHorizontalLimit = ComputeFallbackHorizontalLimit(m_horizontalBoundsMargin);

        if (!m_registry)
        {
            return;
        }

        if (!m_hasCameraBounds)
        {
            m_outOfViewSeconds.clear();
        }

        for (auto& entry : m_registry->MutableStateMap())
        {
            MeteorsState bucketState = entry.first;
            auto& meteorsList = entry.second;

            auto it = meteorsList.begin();
            while (it != meteorsList.end())
            {
                std::shared_ptr<ActiveMeteors> meteors = *it;

                if (!meteors)
                {
#if defined(_DEBUG)
                    std::ostringstream oss;
                    oss << kLogPrefix << "状態 " << StateToString(bucketState)
                        << " から null のスイーツを掃除したよ\n";
                    DebugTrace(oss.str());
#endif
                    it = meteorsList.erase(it);
                    continue;
                }

                if (meteors->IsPendingDestroy())
                {
                    m_outOfViewSeconds.erase(meteors.get());
                    ++it;
                    continue;
                }

                if (!meteors->IsEnabled())
                {
#if defined(_DEBUG)
                    std::ostringstream oss;
                    oss << kLogPrefix << "Update 無効化中だったから、状態 " << StateToString(bucketState)
                        << " から meteors ptr=" << meteors.get() << " を片付けたよ \n";
                    DebugTrace(oss.str());
#endif
                    m_outOfViewSeconds.erase(meteors.get());
                    m_registry->Lookup().erase(meteors);
                    it = meteorsList.erase(it);
                    continue;
                }

                const bool isCarried = (bucketState == MeteorsState::Carried);

                if (!isCarried)
                {
                    if (m_hasCameraBounds)
                    {
                        meteors->SyncColliderBounds();
                        const AABB bounds = meteors->GetAABB();
                        const bool isVisible = !(bounds.max.x < m_cameraMinX
                            || bounds.min.x > m_cameraMaxX
                            || bounds.max.y < m_cameraMinY
                            || bounds.min.y > m_cameraMaxY);
                        if (!isVisible)
                        {
                            float& timer = m_outOfViewSeconds[meteors.get()];
                            timer += deltaSeconds;
                            if (timer >= kOutOfViewTimeoutSeconds)
                            {
                                m_outOfViewSeconds.erase(meteors.get());
                                RemoveMeteorsEntry(meteors, DeleteReason::Miss, "OutOfViewDespawn", true, nullptr);
                                ++it;
                                continue;
                            }
                        }
                        else
                        {
                            m_outOfViewSeconds.erase(meteors.get());
                        }
                    }

                    if (m_horizontalWrapEnabled)
                    {
                        bool usedFallbackBounds = false;
                        bool wasOutOfBounds = false;
                        const bool wrapped = WrapMeteorsHorizontally(
                            *meteors,
                            horizontalBounds,
                            m_horizontalBoundsMargin,
                            fallbackHorizontalLimit,
                            &usedFallbackBounds,
                            &wasOutOfBounds);
                        if (usedFallbackBounds && !m_warnedMissingHorizontalBounds)
                        {
                            std::ostringstream oss;
                            oss << kLogPrefix << "水平境界のフォールバックを使うね 幅制限="
                                << fallbackHorizontalLimit << " (半幅＋マージン) \n";
                            DebugTrace(oss.str());
                            m_warnedMissingHorizontalBounds = true;
                        }

                        if (wasOutOfBounds)
                        {
                            if (!wrapped)
                            {
                                missedMeteors.push_back(meteors);
                            }
                            ++it;
                            continue;
                        }
                    }

                    const MeteorsState desiredState = EvaluateState(*meteors, bucketState, *m_contacts);
                    if (desiredState != bucketState)
                    {
                        stateChanges.push_back({ meteors, desiredState });
                    }
                }

                ++it;
            }
        }

        for (const auto& change : stateChanges)
        {
            auto lookup = m_registry->Lookup().find(change.meteors);
            const MeteorsState previousState = (lookup != m_registry->Lookup().end()) ? lookup->second : MeteorsState::Active;
            m_registry->SetState(change.meteors, change.state);
            if (m_contacts)
            {
                m_contacts->OnStateChanged(change.meteors.get(), previousState, change.state);
            }
            if (m_spawner)
            {
                m_spawner->OnStateChanged(change.meteors, previousState, change.state);
            }
#if defined(_DEBUG)
            std::ostringstream oss;
            oss << kLogPrefix << "状態変更 ptr=" << change.meteors.get()
                << " " << StateToString(previousState) << " -> "
                << StateToString(change.state) << "\n";
            DebugTrace(oss.str());
#endif
        }

        DeleteBatchContext missBatch{};
        missBatch.usesRemaining = static_cast<int>(missedMeteors.size());
        for (const auto& meteors : missedMeteors)
        {
            RemoveMeteorsEntry(meteors, DeleteReason::Miss, "MissDespawn", true, &missBatch);
        }
    }

    void MeteorsLifecycle::OnLateUpdate(const Core::UpdateContext& ctx)
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
            auto& meteorsList = entry.second;
            auto it = meteorsList.begin();
            while (it != meteorsList.end())
            {
                const auto& meteors = *it;
                if (!meteors)
                {
                    it = meteorsList.erase(it);
                    ++removed;
                    continue;
                }

                if (meteors->IsPendingDestroy())
                {
                    auto watched = m_pendingLateDeletion.find(meteors);
                    if (watched == m_pendingLateDeletion.end())
                    {
                        m_pendingLateDeletion.insert(meteors);
                        ++stagedThisFrame;
                        ++it;
                        continue;
                    }

                    if (!meteors->IsSafeToDelete())
                    {
                        meteors->MarkSafeToDelete();
                        ++markedSafe;
                        ++it;
                        continue;
                    }

                    m_registry->Lookup().erase(meteors);
                    m_pendingLateDeletion.erase(watched);
                    m_outOfViewSeconds.erase(meteors.get());
                    it = meteorsList.erase(it);
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

    void MeteorsLifecycle::SetHorizontalBounds(float minX, float maxX, std::optional<float> margin)
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

    std::optional<std::pair<float, float>> MeteorsLifecycle::GetHorizontalBounds() const
    {
        if (!m_hasHorizontalBounds)
        {
            return std::nullopt;
        }

        return std::make_pair(m_minHorizontalBound, m_maxHorizontalBound);
    }

    void MeteorsLifecycle::SetCameraBounds(float minX, float maxX, float minY, float maxY)
    {
        if (!(std::isfinite(minX) && std::isfinite(maxX) && std::isfinite(minY) && std::isfinite(maxY)))
        {
            DebugTrace(std::string(kLogPrefix) + "有限値じゃないカメラ境界の更新は無視したよ \n");
            return;
        }

        m_hasCameraBounds = true;
        m_cameraMinX = std::min(minX, maxX);
        m_cameraMaxX = std::max(minX, maxX);
        m_cameraMinY = std::min(minY, maxY);
        m_cameraMaxY = std::max(minY, maxY);
    }

    void MeteorsLifecycle::Erase(const std::shared_ptr<ActiveMeteors>& meteors)
    {
        RemoveMeteorsEntry(meteors, DeleteReason::Hit, "PlayerHit", true, nullptr);
    }

    void MeteorsLifecycle::ClearAllMeteors()
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
            m_contacts->ClearCreamWeldJoints("ClearAllMeteors");
        }

        std::size_t staged = 0;
        std::size_t alreadyPending = 0;
        m_outOfViewSeconds.clear();

        for (auto& entry : m_registry->MutableStateMap())
        {
            auto& list = entry.second;
            for (auto& meteors : list)
            {
                if (!meteors)
                {
                    continue;
                }

                if (meteors->IsPendingDestroy())
                {
                    ++alreadyPending;
                }
                else
                {
                    meteors->MarkPendingDestroy();
                    meteors->Enable(false);
                    ++staged;
                }

                UnregisterMeteorsFromSystems(meteors, "ClearAllMeteors", true);
            }
        }

        std::ostringstream oss;
        oss << kLogPrefix << "ClearAllMeteors で " << staged
            << " 個を破棄予約したよ（すでに予約済み " << alreadyPending << " 個）/ "
            << m_registry->StateMap().size() << " 状態分 \n";
        DebugTrace(oss.str());
    }

    bool MeteorsLifecycle::TryMarkAsCarried(const std::shared_ptr<ActiveMeteors>& meteors)
    {
        if (!meteors || !m_registry)
        {
            return false;
        }

        if (meteors->IsPendingDestroy())
        {
            return false;
        }

        meteors->SetWasEverCarried(true);
        if (m_contacts)
        {
            m_contacts->ClearContactState(meteors.get());
            m_contacts->RemoveCreamWeldJointsForMeteors(meteors.get(), "TryMarkAsCarried");
        }

        if (m_physicsMachine)
        {
            m_physicsMachine->UnregisterBody(*meteors, true);
        }
        else if (m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
        {
            DebugTrace(std::string(kLogPrefix) + "TryMarkAsCarried で物理マシンが見つからないから、このままシミュレーションしちゃうかも \n");
            *m_warnedMissingPhysicsMachine = true;
        }

        auto previous = m_registry->SetState(meteors, MeteorsState::Carried);
        if (previous && m_contacts)
        {
            m_contacts->OnStateChanged(meteors.get(), *previous, MeteorsState::Carried);
        }

        std::ostringstream oss;
        oss << kLogPrefix << "TryMarkAsCarried 成功 tag='" << meteors->GetTag() << "'\n";
        DebugTrace(oss.str());
        return true;
    }

    bool MeteorsLifecycle::ReleaseCarriedMeteors(const std::shared_ptr<ActiveMeteors>& meteors)
    {
        if (!meteors || !m_registry)
        {
            return false;
        }

        if (meteors->IsPendingDestroy())
        {
            return false;
        }

        auto it = m_registry->Lookup().find(meteors);
        if (it == m_registry->Lookup().end() || it->second != MeteorsState::Carried)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "ReleaseCarriedMeteors 却下 tag='" << meteors->GetTag() << "' state="
                << (it == m_registry->Lookup().end() ? "<unknown>" : StateToString(it->second)) << "\n";
            DebugTrace(oss.str());
            return false;
        }

        auto previous = m_registry->SetState(meteors, MeteorsState::Active);
        if (previous && m_contacts)
        {
            m_contacts->OnStateChanged(meteors.get(), *previous, MeteorsState::Active);
        }
        if (m_contacts)
        {
            m_contacts->RemoveCreamWeldJointsForMeteors(meteors.get(), "ReleaseCarriedMeteors");
        }

        if (m_physicsMachine)
        {
            m_physicsMachine->RegisterBody(*meteors);
        }
        else if (m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
        {
            DebugTrace(std::string(kLogPrefix) + "ReleaseCarriedMeteors で物理マシンがないから、ボディがシミュレーションされないよ \n");
            *m_warnedMissingPhysicsMachine = true;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "ReleaseCarriedMeteors 成功 tag='" << meteors->GetTag() << "'\n";
        DebugTrace(oss.str());
        return true;
    }

    bool MeteorsLifecycle::RegisterWithSystems(const std::shared_ptr<ActiveMeteors>& meteors)
    {
        if (!meteors)
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
                m_contacts->ClearContactState(meteors.get());
            }
            meteors->MarkPendingDestroy();
            meteors->Enable(false);
            UnregisterMeteorsFromSystems(meteors, "RegisterWithSystems", false);

            if (m_registry)
            {
                for (auto& entry : m_registry->MutableStateMap())
                {
                    entry.second.remove(meteors);
                }
                m_registry->Lookup().erase(meteors);
            }
            return false;
        };

        if (m_scheduler)
        {
            auto handle = m_scheduler->Register(*meteors);
            if (!handle.IsValid())
            {
                DebugLog::DebugMessage("[MeteorsLifecycle] Scheduler registration failed for meteors.\n");
                assert(false && "Meteors scheduler registration failed.");
                return cleanupFailedRegistration("スケジューラ登録に失敗したよ");
            }
            meteors->SetRegistrationHandle(std::move(handle));
        }
        else if (m_warnedMissingScheduler && !*m_warnedMissingScheduler)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "スイーツ '" << meteors->GetTag()
                << "' をスケジューラに登録できなかったよ スケジューラが設定されてないみたい \n";
            DebugTrace(oss.str());
            *m_warnedMissingScheduler = true;
        }

        if (m_physicsMachine)
        {
            const auto& colliderComponent = meteors->GetBoxColliderComponent();
            const bool hasColliderComponent = static_cast<bool>(colliderComponent);

            if (!hasColliderComponent)
            {
                std::ostringstream oss;
                oss << "スイーツ '" << meteors->GetTag() << "' の物理登録を飛ばすね コライダー設定が見当たらない";
                return cleanupFailedRegistration(oss.str());
            }

            const bool alreadyRegistered = m_registeredPhysicsBodies.find(meteors.get()) != m_registeredPhysicsBodies.end()
                || b2Body_IsValid(m_physicsMachine->FindBody(*meteors));
            if (!alreadyRegistered)
            {
                m_physicsMachine->RegisterBody(*meteors);
            }

            const int fixtureCount = colliderComponent ? colliderComponent->createdFixtureCount : 0;
            const bool hasBody = meteors->HasBox2DBody();

            if (!hasBody || fixtureCount <= 0)
            {
                std::ostringstream oss;
                oss << "スイーツ '" << meteors->GetTag() << "' の物理登録に失敗 hasBody=" << std::boolalpha << hasBody
                    << " fixtures=" << fixtureCount;
                return cleanupFailedRegistration(oss.str());
            }

            m_registeredPhysicsBodies.insert(meteors.get());
        }
        else if (m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "スイーツ '" << meteors->GetTag()
                << "' を物理に登録できなかったよ 物理マシンが設定されてない \n";
            DebugTrace(oss.str());
            *m_warnedMissingPhysicsMachine = true;
        }

        if (m_drawSystem)
        {
            m_drawSystem->RegisterRenderable(*meteors);
        }
        else if (m_warnedMissingDrawSystem && !*m_warnedMissingDrawSystem)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "スイーツ '" << meteors->GetTag()
                << "' を描画登録できなかったよ ドローシステムが設定されてない \n";
            DebugTrace(oss.str());
            *m_warnedMissingDrawSystem = true;
        }

        return true;
    }

    void MeteorsLifecycle::RemoveMeteorsEntry(const std::shared_ptr<ActiveMeteors>& meteors,
        DeleteReason reason,
        const char* context,
        bool logWarnings,
        DeleteBatchContext* batchContext)
    {
        if (!meteors || !m_registry)
        {
            return;
        }

        auto found = m_registry->Lookup().find(meteors);
        if (found == m_registry->Lookup().end())
        {
            return;
        }

        if (meteors->IsPendingDestroy())
        {
            return;
        }

        m_outOfViewSeconds.erase(meteors.get());
        const MeteorsState previousState = found->second;
        if (m_contacts)
        {
            m_contacts->ClearContactState(meteors.get());
            m_contacts->ClearMeteorsContact(meteors.get());
            m_contacts->RemoveCreamWeldJointsForMeteors(meteors.get(), context);
        }
        meteors->SetWasEverCarried(false);
        meteors->MarkPendingDestroy();
        meteors->Enable(false);

        const UseOutcome outcome = (reason == DeleteReason::Hit) ? UseOutcome::Hit : UseOutcome::Miss;
        UpdateBatchUsage(batchContext, outcome);

        UnregisterMeteorsFromSystems(meteors, context, logWarnings);

        if (m_spawner)
        {
            const int remainingToStack = std::max(0, m_spawner->GetBatchStackTarget() - m_spawner->GetStackedInBatch());
            const int liveAfterRemoval = m_spawner->CountLiveMeteors();
            m_spawner->OnMeteorsRemoved(remainingToStack, liveAfterRemoval, context);
        }

#if defined(_DEBUG)
        const auto& pos = meteors->GetTransform().position;
        std::ostringstream oss;
        oss << kLogPrefix << context << " でスイーツ tag='" << meteors->GetTag()
            << "' を削除 state=" << StateToString(previousState)
            << " deleteReason=" << DeleteReasonToString(reason)
            << " outcome=" << UseOutcomeToString(outcome)
            << " batchUsesRemaining=" << (batchContext ? batchContext->usesRemaining : -1)
            << " pos=(" << pos.x << ',' << pos.y << ',' << pos.z << ")\n";
        DebugTrace(oss.str());
#endif
    }

    void MeteorsLifecycle::UnregisterMeteorsFromSystems(const std::shared_ptr<ActiveMeteors>& meteors, const char* context, bool logWarnings)
    {
        if (!meteors)
        {
            return;
        }

        if (m_contacts)
        {
            m_contacts->RemoveCreamWeldJointsForMeteors(meteors.get(), context);
        }

        if (m_scheduler)
        {
            meteors->ReleaseRegistrationHandle();
        }
        else if (logWarnings && m_warnedMissingScheduler && !*m_warnedMissingScheduler)
        {
            std::ostringstream oss;
            oss << kLogPrefix << context << " でスイーツ '" << meteors->GetTag()
                << "' をスケジューラから外せなかったよ スケジューラ未設定 \n";
            DebugTrace(oss.str());
            *m_warnedMissingScheduler = true;
        }

        if (m_physicsMachine)
        {
            const bool tracked = m_registeredPhysicsBodies.erase(meteors.get()) > 0;
            if (tracked || b2Body_IsValid(m_physicsMachine->FindBody(*meteors)))
            {
                m_physicsMachine->UnregisterBody(*meteors);
            }
        }
        else if (logWarnings && m_warnedMissingPhysicsMachine && !*m_warnedMissingPhysicsMachine)
        {
            std::ostringstream oss;
            oss << kLogPrefix << context << " でスイーツ '" << meteors->GetTag()
                << "' の物理登録を外せなかったよ 物理マシン未設定 \n";
            DebugTrace(oss.str());
            *m_warnedMissingPhysicsMachine = true;
        }

        if (m_drawSystem)
        {
            m_drawSystem->UnregisterRenderable(*meteors);
        }
        else if (logWarnings && m_warnedMissingDrawSystem && !*m_warnedMissingDrawSystem)
        {
            std::ostringstream oss;
            oss << kLogPrefix << context << " でスイーツ '" << meteors->GetTag()
                << "' の描画登録を外せなかったよ ドローシステム未設定 \n";
            DebugTrace(oss.str());
            *m_warnedMissingDrawSystem = true;
        }
    }
}
