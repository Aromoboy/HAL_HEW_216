#include "SweetsManager.h"

#include <sstream>

#include <box2d/box2d.h>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

#include "Core/UpdateScheduler.h"
#include "Sweets/BaseSweets.h"
#include "Systems/Physics/PhysicsMachine.h"

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
        , m_phaseChangeCallback()
        , m_currentPhase(0)
        , m_warnedMissingDrawSystem(false)
        , m_warnedMissingPhysicsMachine(false)
        , m_warnedMissingScheduler(false)
        , m_warnedMissingPhaseCallback(false)
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
    }

    void SweetsManager::Erase(const std::shared_ptr<ActiveSweets>& sweets)
    {
        m_lifecycle.Erase(sweets);
    }

    void SweetsManager::ClearAllSweets()
    {
        m_lifecycle.ClearAllSweets();
    }

    void SweetsManager::SetPhaseChangeCallback(PhaseChangeCallback callback)
    {
        m_phaseChangeCallback = std::move(callback);
        m_warnedMissingPhaseCallback = false;
    }

    void SweetsManager::NotifyPhaseChanged(int newPhase)
    {
        if (newPhase < 0)
        {
            newPhase = 0;
        }

        if (newPhase == m_currentPhase)
        {
            return;
        }

        m_currentPhase = newPhase;

        std::ostringstream oss;
        oss << kLogPrefix << "フェーズが " << m_currentPhase << " に変わったよ\n";
        DebugTrace(oss.str());

        m_spawner.NotifyPhaseChanged(m_currentPhase);

        if (m_phaseChangeCallback)
        {
            m_phaseChangeCallback(m_currentPhase);
        }
        else if (!m_warnedMissingPhaseCallback)
        {
            DebugTrace(std::string(kLogPrefix) + "フェーズ変更のコールバックがないので、スポーン配置はそのままにしとくね \n");
            m_warnedMissingPhaseCallback = true;
        }
    }

    int SweetsManager::GetCurrentPhase() const noexcept
    {
        return m_currentPhase;
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
        return m_lifecycle.ReleaseCarriedSweets(sweets);
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

    int SweetsManager::GetSweetsCount() const
    {
        return m_registry.CountAll();
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

    bool SweetsManager::LoadPhaseSpawnConfig(const std::string& path)
    {
        return m_spawner.LoadPhaseSpawnConfig(path);
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

    void SweetsManager::SetSpawnLimitsConfigured(bool configured)
    {
        m_spawner.SetSpawnLimitsConfigured(configured);
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
