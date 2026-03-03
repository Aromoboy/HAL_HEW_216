#include "SweetsManager.h"

#include <box2d/box2d.h>

#include <algorithm>

#include <cmath>
#include <limits>
#include <sstream>

#include "Core/UpdateScheduler.h"
#include "Sweets/BaseSweets.h"
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
    constexpr int kLogRepeatSuppressionFrames = 15;

    struct RepeatedLogGate
    {
        int lastValue = std::numeric_limits<int>::min();
        int suppressCount = 0;

        bool ShouldLog(int value)
        {
            if (value != lastValue)
            {
                lastValue = value;
                suppressCount = 0;
                return true;
            }

            ++suppressCount;
            if (suppressCount >= kLogRepeatSuppressionFrames)
            {
                suppressCount = 0;
                return true;
            }

            return false;
        }
    };

    std::string NormalizeTag(const std::string& tag)
    {
        return SweetsSpace::NormalizeKey(tag);
    }

    bool IsCreamTag(const std::string& tag)
    {
        static const std::string kCreamKey = NormalizeTag(::SweetsSpace::ToString(::SweetsSpace::SweetsType::Cream));
        return NormalizeTag(tag) == kCreamKey;
    }

    bool IsCreamKind(const std::string& kind)
    {
        const auto resolved = ::SweetsSpace::SweetsPrototypeManifest::Instance().Resolve(kind);
        return resolved && *resolved == ::SweetsSpace::SweetsType::Cream;
    }

    bool IsWithinCakeStackZone(const ::SweetsSpace::ActiveSweets& sweets, const std::optional<AABB>& cakeBounds)
    {
        if (!cakeBounds)
        {
            return false;
        }

        auto* mutableSweets = const_cast<::SweetsSpace::ActiveSweets*>(&sweets);
        mutableSweets->SyncColliderBounds();
        const AABB bounds = sweets.GetAABB();

        if (bounds.max.x < cakeBounds->min.x || bounds.min.x > cakeBounds->max.x)
        {
            return false;
        }

        return bounds.min.y >= cakeBounds->max.y;
    }
}

namespace SweetsSpace
{
    class LateUpdateNode : public Core::UpdateNode
    {
    public:
        explicit LateUpdateNode(::SweetsSpace::SweetsManager& owner)
            : Core::UpdateNode(Core::UpdatePhase::Late, 0)
            , m_owner(owner)
        {
        }

        void OnUpdate(const Core::UpdateContext& ctx) override
        {
            m_owner.OnLateUpdate(ctx);
        }

    private:
        ::SweetsSpace::SweetsManager& m_owner;
    };

    SweetsManager::SweetsManager(Systems::DrawSystem& drawSystem,
        Systems::PhysicsMachine& physicsMachine,
        Core::UpdateScheduler& scheduler)
        : m_drawSystem(&drawSystem)
        , m_physicsMachine(&physicsMachine)
        , m_scheduler(&scheduler)
        , m_lateUpdateNode(std::make_unique<LateUpdateNode>(*this))
        , m_warnedMissingDrawSystem(false)
        , m_warnedMissingPhysicsMachine(false)
        , m_warnedMissingScheduler(false)
        , m_registry()
        , m_factory(physicsMachine)
        , m_spawner(m_factory, m_registry, [this](const std::string& kind, const Transform& transform)
            {
                return this->Spawn(kind, transform);
            })
        , m_contacts(m_physicsMachine, &m_warnedMissingPhysicsMachine)
        , m_lifecycle(m_registry, m_contacts, m_spawner, m_drawSystem, m_physicsMachine, m_scheduler,
            &m_warnedMissingDrawSystem, &m_warnedMissingPhysicsMachine, &m_warnedMissingScheduler)
        , m_renderer(m_registry)
    {
    }

    SweetsManager::~SweetsManager()
    {
        m_contacts.ClearCreamWeldJoints("~SweetsManager");
        if (m_scheduler && m_lateUpdateNode)
        {
            m_lateUpdateNode->ReleaseRegistrationHandle();
        }
    }

    void SweetsManager::OnUpdate(const Core::UpdateContext& ctx)
    {
        m_spawner.RunSpawnScheduler(ctx.dt);
        m_lifecycle.OnUpdate(ctx);
        m_contacts.TickDebugStats(ctx.dt);
    }

    void SweetsManager::OnLateUpdate(const Core::UpdateContext& ctx)
    {
        m_contacts.ProcessPendingGlue();
        m_lifecycle.OnLateUpdate(ctx);
    }

    void SweetsManager::Update(const Core::UpdateContext& ctx)
    {
        OnUpdate(ctx);
    }

    void SweetsManager::DrawAll(Render::ModelRenderer& renderer, const std::shared_ptr<::CameraSpace::ICamera>& camera)
    {
        m_renderer.DrawAll(renderer, camera);
    }

    SweetsManager::SweetsList& SweetsManager::GetByState(SweetsState state)
    {
        return m_registry.GetByState(state);
    }

    const SweetsManager::SweetsList& SweetsManager::GetByState(SweetsState state) const
    {
        return m_registry.GetByState(state);
    }

    float SweetsManager::GetHighestStackedHeight() const
    {
        return m_registry.GetHighestStackedHeight();
    }

    void SweetsManager::SetState(const std::shared_ptr<ActiveSweets>& sweets, SweetsState state)
    {
        if (!sweets)
        {
            return;
        }

        if (sweets->IsPendingDestroy())
        {
            return;
        }

        auto previous = m_registry.SetState(sweets, state);
        if (!previous)
        {
            return;
        }

        m_contacts.OnStateChanged(sweets.get(), *previous, state);
        m_spawner.OnStateChanged(sweets, *previous, state);
        if (state == SweetsState::Stacked && *previous != SweetsState::Stacked)
        {
            m_spawner.ReleaseSpawnOriginOnStacked(sweets);
        }
    }

    void SweetsManager::Erase(const std::shared_ptr<ActiveSweets>& sweets)
    {
        m_lifecycle.Erase(sweets);
    }

    void SweetsManager::ClearAllSweets()
    {
        m_lifecycle.ClearAllSweets();
    }
    void SweetsManager::SeedInitialSpawns()
    {
        m_spawner.SeedInitialSpawns();
    }

    std::vector<std::pair<std::string, SweetsManager::FactoryPreset>> SweetsManager::EnumerateFactoryPresets() const
    {
        return m_factory.EnumerateFactoryPresets();
    }

    Core::IUpdatable* SweetsManager::GetLateUpdateNode()
    {
        return m_lateUpdateNode.get();
    }

    const Core::IUpdatable* SweetsManager::GetLateUpdateNode() const
    {
        return m_lateUpdateNode.get();
    }

    std::shared_ptr<ActiveSweets> SweetsManager::Spawn(const std::string& kind, const Transform& transform)
    {
        if (!m_creamEnabled && IsCreamKind(kind))
        {
            return nullptr;
        }

        auto sweets = m_factory.Create(kind, transform);
        if (!sweets)
        {
            return nullptr;
        }

        sweets->SetWasEverCarried(false);

        if (!m_lifecycle.RegisterWithSystems(sweets))
        {
            return nullptr;
        }

        SetState(sweets, SweetsState::Active);

        if (m_physicsMachine && sweets)
        {
            const b2BodyId bodyId = m_physicsMachine->FindBody(*sweets);
            if (b2Body_IsValid(bodyId))
            {
                const b2Vec2 position = b2Body_GetPosition(bodyId);
                Transform synced = sweets->GetTransform();
                synced.position.x = position.x;
                synced.position.y = position.y;
                sweets->SetTransform(synced);
            }
        }

        return sweets;
    }

    PickupQueryResult SweetsManager::FindNearestCarryable(
        const float3& pickupCenter,
        float halfWidth,
        float halfHeight,
        float halfDepth,
        float depthThreshold,
        const std::vector<std::shared_ptr<ActiveSweets>>& exclude,
        float maxDistance) const
    {
        return m_registry.FindNearestCarryable(pickupCenter, halfWidth, halfHeight, halfDepth, depthThreshold, exclude, maxDistance);
    }

    bool SweetsManager::TryMarkAsCarried(const std::shared_ptr<ActiveSweets>& sweets)
    {
        return m_lifecycle.TryMarkAsCarried(sweets);
    }

    bool SweetsManager::ReleaseCarriedSweets(const std::shared_ptr<ActiveSweets>& sweets)
    {
        const bool released = m_lifecycle.ReleaseCarriedSweets(sweets);
        if (!released || !sweets || sweets->IsPendingDestroy())
        {
            return released;
        }

        if (!IsCreamTag(sweets->GetTag()))
        {
            const bool shouldStack = IsAnchoredToCake(*sweets) || IsWithinCakeStackZone(*sweets, m_cakeBounds);
            if (shouldStack)
            {
                SetState(sweets, SweetsState::Stacked);
            }
        }

        return released;
    }

    void SweetsManager::SetOutOfBoundsSettings(bool despawnEnabled, bool respawnEnabled, float respawnDelaySeconds)
    {
        m_lifecycle.SetOutOfBoundsSettings(despawnEnabled, respawnEnabled, respawnDelaySeconds);
    }

    void SweetsManager::SetGrabRespawnDelaySeconds(float respawnDelaySeconds)
    {
        m_lifecycle.SetGrabRespawnDelaySeconds(respawnDelaySeconds);
    }

    void SweetsManager::SetHorizontalBounds(float minX, float maxX, std::optional<float> margin)
    {
        m_lifecycle.SetHorizontalBounds(minX, maxX, margin);
    }

    void SweetsManager::SetWorldBounds(float minX, float maxX, float minY, std::optional<float> margin)
    {
        m_lifecycle.SetWorldBounds(minX, maxX, minY, margin);
    }

    void SweetsManager::SetCakeBounds(const std::optional<AABB>& bounds)
    {
        m_cakeBounds = bounds;
    }

    void SweetsManager::SetGeneratedColliderThicknessPadding(float factor)
    {
        m_factory.SetGeneratedColliderThicknessPadding(factor);
    }

    void SweetsManager::SetGeneratedColliderMinThickness(float thickness)
    {
        m_factory.SetGeneratedColliderMinThickness(thickness);
    }

    void SweetsManager::OnContactBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b,
        const Systems::ContactEventInfo& info)
    {
        m_contacts.OnContactBegin(a, b, info);
    }

    void SweetsManager::OnContactEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b,
        const Systems::ContactEventInfo& info)
    {
        m_contacts.OnContactEnd(a, b, info);
    }

    bool SweetsManager::HasSupportingContact(const ActiveSweets& sweets, bool* foundationOnly) const
    {
        return m_contacts.HasSupportingContact(sweets, foundationOnly);
    }

    bool SweetsManager::HasEverCollidedWithSweets(const ActiveSweets& sweets) const
    {
        return m_contacts.HasEverCollidedWithSweets(sweets);
    }

    bool SweetsManager::IsAnchoredToCake(const ActiveSweets& sweets) const
    {
        return m_contacts.IsAnchoredToCake(sweets);
    }

    void SweetsManager::SetCreamEnabled(bool enabled)
    {
        m_contacts.SetCreamEnabled(enabled);
        m_spawner.SetCreamEnabled(enabled);
        m_creamEnabled = enabled;
        if (!enabled)
        {
            std::vector<std::shared_ptr<ActiveSweets>> toRemove;
            toRemove.reserve(m_registry.Lookup().size());
            for (const auto& entry : m_registry.Lookup())
            {
                const auto& sweets = entry.first;
                if (sweets && IsCreamTag(sweets->GetTag()))
                {
                    toRemove.push_back(sweets);
                }
            }

            for (const auto& sweets : toRemove)
            {
                m_lifecycle.Erase(sweets);
            }
        }
    }

    int SweetsManager::GetSweetsCount() const
    {
        return m_registry.CountAll();
    }

    void SweetsManager::NotifyStageMoved(float minX, float maxX, float minY, float maxY, float deltaY)
    {
        if (!m_physicsMachine || std::fabs(deltaY) <= 1e-5f)
        {
            return;
        }

        const float sanitizedMinX = std::min(minX, maxX);
        const float sanitizedMaxX = std::max(minX, maxX);
        const float sanitizedMinY = std::min(minY, maxY);
        const float sanitizedMaxY = std::max(minY, maxY);

        constexpr float kVerticalNearThreshold = 0.25f;
        constexpr float kHorizontalNearThreshold = 0.35f;

        std::vector<std::shared_ptr<ActiveSweets>> candidates;
        candidates.reserve(m_registry.CountAll());

        auto collectState = [&](SweetsState state)
        {
            for (const auto& sweets : m_registry.GetByState(state))
            {
                if (!sweets || sweets->IsPendingDestroy())
                {
                    continue;
                }
                candidates.push_back(sweets);
            }
        };

        collectState(SweetsState::Active);
        collectState(SweetsState::Ground);

        int affectedCount = 0;
        int wokeCount = 0;
        int suspendedCandidates = 0;

        for (const auto& sweets : candidates)
        {
            sweets->SyncColliderBounds();
            const AABB bounds = sweets->GetAABB();

            const bool overlapX = bounds.min.x <= sanitizedMaxX && bounds.max.x >= sanitizedMinX;
            const bool overlapY = bounds.min.y <= sanitizedMaxY && bounds.max.y >= sanitizedMinY;
            const bool overlap = overlapX && overlapY;

            const bool nearHorizontal = overlapX
                && (std::fabs(bounds.min.y - sanitizedMaxY) <= kVerticalNearThreshold
                    || std::fabs(bounds.max.y - sanitizedMinY) <= kVerticalNearThreshold);
            const bool nearVertical = overlapY
                && (std::fabs(bounds.min.x - sanitizedMaxX) <= kHorizontalNearThreshold
                    || std::fabs(bounds.max.x - sanitizedMinX) <= kHorizontalNearThreshold);

            if (!(overlap || nearHorizontal || nearVertical))
            {
                continue;
            }

            ++affectedCount;
            const auto wakeStatus = m_physicsMachine->WakeBody(*sweets, true);
            if (wakeStatus == Systems::PhysicsMachine::WakeBodyStatus::Woke)
            {
                ++wokeCount;
            }
            else if (wakeStatus == Systems::PhysicsMachine::WakeBodyStatus::NoBody)
            {
                ++suspendedCandidates;
            }

            m_lifecycle.MarkForStateReevaluation(sweets);
        }

#if defined(_DEBUG)
        static RepeatedLogGate osaraDeltaGate;
        static RepeatedLogGate affectedGate;
        static RepeatedLogGate wokeGate;
        static RepeatedLogGate suspendedGate;

        const int deltaMilli = static_cast<int>(std::lround(deltaY * 1000.0f));
        if (osaraDeltaGate.ShouldLog(deltaMilli))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "osara moved +ΔY=" << deltaY << "\n";
            DebugTrace(oss.str());
        }
        if (affectedGate.ShouldLog(affectedCount))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "affected sweets count=" << affectedCount << "\n";
            DebugTrace(oss.str());
        }
        if (wokeGate.ShouldLog(wokeCount))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "woke sweets count=" << wokeCount << "\n";
            DebugTrace(oss.str());
        }
        if (suspendedGate.ShouldLog(suspendedCandidates))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "still suspended candidates=" << suspendedCandidates << "\n";
            DebugTrace(oss.str());
        }
#endif
    }

    void SweetsManager::OnSweetsPassedGuideline(const std::shared_ptr<ActiveSweets>& sweets)
    {
        if (!sweets)
        {
            return;
        }

        const auto lookup = m_registry.Lookup().find(sweets);
        const SweetsState currentState = (lookup != m_registry.Lookup().end()) ? lookup->second : SweetsState::Active;
        if (currentState != SweetsState::Stacked)
        {
            SetState(sweets, SweetsState::Stacked);
        }
    }

    bool SweetsManager::GetGlueDebugSnapshot(SweetsContacts::GlueDebugSnapshot& outSnapshot) const
    {
        outSnapshot = m_contacts.BuildDebugSnapshot();
        return true;
    }

    bool SweetsManager::LoadSpawnTuningConfig(const std::string& path)
    {
        return m_spawner.LoadSpawnTuningConfig(path);
    }

    void SweetsManager::SetSpawnOrigins(
        const std::vector<float3>& primary,
        const std::vector<float3>& secondary,
        std::optional<float> spawnHeightOffset)
    {
        m_spawner.SetSpawnOrigins(primary, secondary, spawnHeightOffset);
    }

    void SweetsManager::OffsetSpawnOriginsY(float deltaY)
    {
        m_spawner.OffsetSpawnOriginsY(deltaY);
    }

    void SweetsManager::SetSpawnIntervalMultiplier(float multiplier)
    {
        m_spawner.SetSpawnIntervalMultiplier(multiplier);
    }

    void SweetsManager::SetSpawnCountBonus(int bonusCount)
    {
        m_spawner.SetSpawnCountBonus(bonusCount);
    }

    void SweetsManager::SetRandomRotationRangeDegrees(const float3& minDegrees, const float3& maxDegrees)
    {
        m_spawner.SetRandomRotationRangeDegrees(minDegrees, maxDegrees);
    }

    void SweetsManager::SetLiveSweetsCap(int cap)
    {
        m_spawner.SetLiveSweetsCap(cap);
    }

    void SweetsManager::SetSpawnPointCap(int cap)
    {
        m_spawner.SetSpawnPointCap(cap);
    }

    void SweetsManager::SetSpawnPerPointInitial(int count)
    {
        m_spawner.SetSpawnPerPointInitial(count);
    }

    void SweetsManager::SetForceGeneratedColliders(bool force)
    {
        m_factory.SetForceGeneratedColliders(force);
    }

    void SweetsManager::SetForceConfigColliders(bool force, const std::vector<std::string>& kinds)
    {
        m_factory.SetForceConfigColliders(force, kinds);
    }

    void SweetsManager::SetColliderConfigOverridesEnabled(bool enabled)
    {
        m_factory.SetColliderConfigOverridesEnabled(enabled);
    }

    void SweetsManager::ForceSpawnRefresh()
    {
        m_spawner.ForceSpawnRefresh();
    }
}
