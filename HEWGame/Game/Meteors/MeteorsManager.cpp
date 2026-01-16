#include "MeteorsManager.h"

#include <cmath>
#include <sstream>

#include <box2d/box2d.h>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

#include "Core/UpdateScheduler.h"
#include "Meteors/BaseMeteors.h"
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

    constexpr const char* kLogPrefix = "[MeteorsManager] ";
}

namespace MeteorsSpace
{
    class LateUpdateNode : public Core::UpdateNode
    {
    public:
        explicit LateUpdateNode(::MeteorsSpace::MeteorsManager& owner)
            : Core::UpdateNode(Core::UpdatePhase::Late, 0)
            , m_owner(owner)
        {
        }

        void OnUpdate(const Core::UpdateContext& ctx) override
        {
            m_owner.OnLateUpdate(ctx);
        }

    private:
        ::MeteorsSpace::MeteorsManager& m_owner;
    };

    MeteorsManager::MeteorsManager(Systems::DrawSystem& drawSystem,
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
        , m_random(std::random_device{}())
    {
    }

    MeteorsManager::~MeteorsManager()
    {
        m_contacts.ClearCreamWeldJoints("~MeteorsManager");
        if (m_scheduler && m_lateUpdateNode)
        {
            m_lateUpdateNode->ReleaseRegistrationHandle();
        }
    }

    void MeteorsManager::OnUpdate(const Core::UpdateContext& ctx)
    {
        m_spawner.RunSpawnScheduler(ctx.dt);
        m_lifecycle.OnUpdate(ctx);
        m_contacts.TickDebugStats(ctx.dt);
    }

    void MeteorsManager::OnLateUpdate(const Core::UpdateContext& ctx)
    {
        m_contacts.ProcessPendingGlue();
        m_lifecycle.OnLateUpdate(ctx);
    }

    void MeteorsManager::Update(const Core::UpdateContext& ctx)
    {
        OnUpdate(ctx);
    }

    void MeteorsManager::DrawAll(Render::ModelRenderer& renderer, const std::shared_ptr<::CameraSpace::ICamera>& camera)
    {
        m_renderer.DrawAll(renderer, camera);
    }

    MeteorsManager::MeteorsList& MeteorsManager::GetByState(MeteorsState state)
    {
        return m_registry.GetByState(state);
    }

    const MeteorsManager::MeteorsList& MeteorsManager::GetByState(MeteorsState state) const
    {
        return m_registry.GetByState(state);
    }

    std::shared_ptr<ActiveMeteors> MeteorsManager::FindByEntity(Entities::PhysicalEntity* entity) const
    {
        if (!entity)
        {
            return nullptr;
        }

        const MeteorsState states[] = {
            MeteorsState::Active,
            MeteorsState::Stacked,
            MeteorsState::Ground,
            MeteorsState::Carried
        };

        for (MeteorsState state : states)
        {
            const auto& list = m_registry.GetByState(state);
            for (const auto& meteors : list)
            {
                if (meteors && meteors.get() == entity)
                {
                    return meteors;
                }
            }
        }

        return nullptr;
    }

    float MeteorsManager::GetHighestStackedHeight() const
    {
        return m_registry.GetHighestStackedHeight();
    }

    void MeteorsManager::SetState(const std::shared_ptr<ActiveMeteors>& meteors, MeteorsState state)
    {
        if (!meteors)
        {
            return;
        }

        if (meteors->IsPendingDestroy())
        {
            return;
        }

        auto previous = m_registry.SetState(meteors, state);
        if (!previous)
        {
            return;
        }

        m_contacts.OnStateChanged(meteors.get(), *previous, state);
        m_spawner.OnStateChanged(meteors, *previous, state);
    }

    void MeteorsManager::Erase(const std::shared_ptr<ActiveMeteors>& meteors)
    {
        if (meteors)
        {
            m_meteorsHitCounts.erase(meteors.get());
        }
        m_lifecycle.Erase(meteors);
    }

    void MeteorsManager::ClearAllMeteors()
    {
        m_meteorsHitCounts.clear();
        m_lifecycle.ClearAllMeteors();
    }

    void MeteorsManager::SetPhaseChangeCallback(PhaseChangeCallback callback)
    {
        m_phaseChangeCallback = std::move(callback);
        m_warnedMissingPhaseCallback = false;
    }

    void MeteorsManager::NotifyPhaseChanged(int newPhase)
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

    int MeteorsManager::GetCurrentPhase() const noexcept
    {
        return m_currentPhase;
    }

    std::vector<std::pair<std::string, MeteorsManager::FactoryPreset>> MeteorsManager::EnumerateFactoryPresets() const
    {
        return m_factory.EnumerateFactoryPresets();
    }

    Core::IUpdatable* MeteorsManager::GetLateUpdateNode()
    {
        return m_lateUpdateNode.get();
    }

    const Core::IUpdatable* MeteorsManager::GetLateUpdateNode() const
    {
        return m_lateUpdateNode.get();
    }

    std::shared_ptr<ActiveMeteors> MeteorsManager::Spawn(const std::string& kind, const Transform& transform)
    {
        auto meteors = m_factory.Create(kind, transform);
        if (!meteors)
        {
            return nullptr;
        }

        meteors->SetWasEverCarried(false);

        if (!m_lifecycle.RegisterWithSystems(meteors))
        {
            return nullptr;
        }

        meteors->SetGravityScale(0.0f);
        {
            std::ostringstream log;
            log << kLogPrefix << "Spawned meteors gravityScale=" << meteors->GetGravityScale() << "\n";
            DebugTrace(log.str());
        }

        SetState(meteors, MeteorsState::Active);

        if (m_physicsMachine && meteors)
        {
            const b2BodyId bodyId = m_physicsMachine->FindBody(*meteors);
            if (b2Body_IsValid(bodyId))
            {
                const b2Vec2 position = b2Body_GetPosition(bodyId);
                Transform synced = meteors->GetTransform();
                synced.position.x = position.x;
                synced.position.y = position.y;
                meteors->SetTransform(synced);

                const auto& configs = m_spawner.GetPhaseConfigs();
                const int activeIndex = m_spawner.GetActivePhaseConfigIndex();
                if (activeIndex >= 0 && activeIndex < static_cast<int>(configs.size()))
                {
                    const PhaseSpawnConfig& config = configs[activeIndex];
                    const auto resolvedType = MeteorsPrototypeManifest::Instance().Resolve(kind);
                    float speed = 0.0f;
                    if (resolvedType)
                    {
                        switch (*resolvedType)
                        {
                        case MeteorsType::Mercury:
                            speed = config.mercurySpeed;
                            break;
                        case MeteorsType::Meteor:
                            speed = config.meteorSpeed;
                            break;
                        case MeteorsType::Meteorite:
                            speed = config.meteoriteSpeed;
                            break;
                        }
                    }

                    if (speed > 0.0f)
                    {
                        constexpr float kTwoPi = 6.28318530718f;
                        std::uniform_real_distribution<float> angleDist(0.0f, kTwoPi);
                        const float angle = angleDist(m_random);
                        const b2Vec2 velocity{ std::cos(angle) * speed, std::sin(angle) * speed };
                        b2Body_SetLinearVelocity(bodyId, velocity);
                    }
                }
            }
        }

        return meteors;
    }

    PickupQueryResult MeteorsManager::FindNearestCarryable(
        const float3& pickupCenter,
        float halfWidth,
        float halfHeight,
        float halfDepth,
        float depthThreshold,
        const std::vector<std::shared_ptr<ActiveMeteors>>& exclude,
        float maxDistance) const
    {
        return m_registry.FindNearestCarryable(pickupCenter, halfWidth, halfHeight, halfDepth, depthThreshold, exclude, maxDistance);
    }

    bool MeteorsManager::TryMarkAsCarried(const std::shared_ptr<ActiveMeteors>& meteors)
    {
        return m_lifecycle.TryMarkAsCarried(meteors);
    }

    bool MeteorsManager::ReleaseCarriedMeteors(const std::shared_ptr<ActiveMeteors>& meteors)
    {
        return m_lifecycle.ReleaseCarriedMeteors(meteors);
    }

    void MeteorsManager::SetHorizontalBounds(float minX, float maxX, std::optional<float> margin)
    {
        m_lifecycle.SetHorizontalBounds(minX, maxX, margin);
    }

    void MeteorsManager::SetCameraBounds(float minX, float maxX, float minY, float maxY)
    {
        m_lifecycle.SetCameraBounds(minX, maxX, minY, maxY);
        m_spawner.SetCameraBounds(minX, maxX, minY, maxY);
    }

    void MeteorsManager::SetGeneratedColliderThicknessPadding(float factor)
    {
        m_factory.SetGeneratedColliderThicknessPadding(factor);
    }

    void MeteorsManager::SetGeneratedColliderMinThickness(float thickness)
    {
        m_factory.SetGeneratedColliderMinThickness(thickness);
    }

    void MeteorsManager::SetPlayerWorldPosition(const float3& position)
    {
        m_cachedPlayerPosition = position;
        m_hasCachedPlayerPosition = true;
        m_spawner.SetPlayerWorldPosition(position);
    }

    std::optional<MeteorsType> MeteorsManager::RegisterProjectileHit(const std::shared_ptr<ActiveMeteors>& meteors)
    {
        if (!meteors || meteors->IsPendingDestroy())
        {
            return std::nullopt;
        }

        const auto& manifest = MeteorsPrototypeManifest::Instance();
        const std::optional<MeteorsType> meteorsType = manifest.Resolve(meteors->GetTag());
        int requiredHits = 1;
        if (meteorsType && *meteorsType == MeteorsType::Mercury)
        {
            requiredHits = 2;
        }

        const int hitCount = ++m_meteorsHitCounts[meteors.get()];
        if (hitCount < requiredHits)
        {
            return std::nullopt;
        }

        m_lifecycle.RemoveMeteorsEntry(meteors, DeleteReason::Hit, "MissileHit", true, nullptr);
        m_meteorsHitCounts.erase(meteors.get());
        return meteorsType;
    }

    void MeteorsManager::OnContactBegin(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b,
        const Systems::ContactEventInfo& info)
    {
        m_contacts.OnContactBegin(a, b, info);
    }

    void MeteorsManager::OnContactEnd(Entities::PhysicalEntity* a, Entities::PhysicalEntity* b,
        const Systems::ContactEventInfo& info)
    {
        m_contacts.OnContactEnd(a, b, info);
    }

    bool MeteorsManager::HasSupportingContact(const ActiveMeteors& meteors, bool* foundationOnly) const
    {
        return m_contacts.HasSupportingContact(meteors, foundationOnly);
    }

    bool MeteorsManager::HasEverCollidedWithMeteors(const ActiveMeteors& meteors) const
    {
        return m_contacts.HasEverCollidedWithMeteors(meteors);
    }

    int MeteorsManager::GetMeteorsCount() const
    {
        return m_registry.CountAll();
    }

    bool MeteorsManager::GetGlueDebugSnapshot(MeteorsContacts::GlueDebugSnapshot& outSnapshot) const
    {
        outSnapshot = m_contacts.BuildDebugSnapshot();
        return true;
    }

    bool MeteorsManager::LoadPhaseSpawnConfig(const std::string& path)
    {
        return m_spawner.LoadPhaseSpawnConfig(path);
    }

    void MeteorsManager::SetSpawnOrigins(
        const std::vector<float3>& primary,
        const std::vector<float3>& secondary,
        std::optional<float> spawnHeightOffset)
    {
        m_spawner.SetSpawnOrigins(primary, secondary, spawnHeightOffset);
    }

    void MeteorsManager::SetRandomRotationRangeDegrees(const float3& minDegrees, const float3& maxDegrees)
    {
        m_spawner.SetRandomRotationRangeDegrees(minDegrees, maxDegrees);
    }

    void MeteorsManager::SetLiveMeteorsCap(int cap)
    {
        m_spawner.SetLiveMeteorsCap(cap);
    }

    void MeteorsManager::SetForceGeneratedColliders(bool force)
    {
        m_factory.SetForceGeneratedColliders(force);
    }

    void MeteorsManager::SetForceConfigColliders(bool force, const std::vector<std::string>& kinds)
    {
        m_factory.SetForceConfigColliders(force, kinds);
    }

    void MeteorsManager::SetColliderConfigOverridesEnabled(bool enabled)
    {
        m_factory.SetColliderConfigOverridesEnabled(enabled);
    }

    void MeteorsManager::ForceSpawnRefresh()
    {
        m_spawner.ForceSpawnRefresh();
    }
}
