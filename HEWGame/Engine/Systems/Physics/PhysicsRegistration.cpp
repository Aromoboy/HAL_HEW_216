#include "Systems/Physics/PhysicsRegistration.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

#include "Entities/PhysicalEntity.h"
#include "Entities/Player.h"
#include "Systems/Physics/Box2DContactListener.h"

#if defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

namespace
{
    constexpr const char* kLogPrefix = "[PhysicsBinder] ";

    void DebugTrace(const std::string& message)
    {
#if defined(_WIN32)
#if defined(_DEBUG)
        ::OutputDebugStringA(message.c_str());
#else
        (void)message;
#endif
#else
        (void)message;
#endif
    }

    void LogImmediateDestroy(const Entities::PhysicalEntity& entity, const char* callsite)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "immediate destroy tag='" << entity.GetTag()
            << "' callsite=" << callsite << "\n";
        DebugTrace(oss.str());
    }
}

namespace Systems
{
    class LateUpdateNode : public Core::UpdateNode
    {
    public:
        explicit LateUpdateNode(PhysicsRegistration& owner)
            : Core::UpdateNode(Core::UpdatePhase::Late, 0)
            , m_owner(owner)
        {
        }

        void OnUpdate(const Core::UpdateContext& ctx) override
        {
            (void)ctx;
            m_owner.FlushPendingUnregisters();
        }

    private:
        PhysicsRegistration& m_owner;
    };

    PhysicsRegistration::PhysicsRegistration(PhysicsMachine& physicsMachine)
        : m_physicsMachine(&physicsMachine)
        , m_lateUpdateNode(std::make_unique<LateUpdateNode>(*this))
    {
        m_contactListener = std::make_shared<Box2DContactListener>(physicsMachine);
        if (m_contactListener)
        {
            m_contactListener->SetBeginContactCallback([this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b)
                {
                    DispatchBeginContact(a, b);
                });
            m_contactListener->SetEndContactCallback([this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b)
                {
                    DispatchEndContact(a, b);
                });
            m_contactListener->SetContactLostCallback([this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b)
                {
                    DispatchLostContact(a, b);
                });
            m_contactListener->SetHitContactCallback([this](Entities::PhysicalEntity& a, Entities::PhysicalEntity& b, float approachSpeed)
                {
                    DispatchHitContact(a, b, approachSpeed);
                });

            m_physicsMachine->SetContactListener(m_contactListener);
            m_contactListener->EnableMeshContactLogging(true);
#if defined(_DEBUG)
            m_contactListener->EnableDroppedEventLogging(true);
#endif
        }
    }

    PhysicsRegistration::~PhysicsRegistration()
    {
        if (m_lateUpdateNode)
        {
            m_lateUpdateNode->ReleaseRegistrationHandle();
        }
    }

    void PhysicsRegistration::SetPlayer(Entities::PhysicalEntity* playerEntity)
    {
        m_playerEntity = playerEntity;
    }

    void PhysicsRegistration::RegisterPhysicsBody(Entities::PhysicalEntity& entity)
    {
        if (!m_physicsMachine)
        {
            return;
        }

        const bool isPlayerEntity = (m_playerEntity && m_playerEntity == &entity);
        const auto& boxColliderComponent = entity.GetBoxColliderComponent();
        const bool hasBoxColliderComponent = boxColliderComponent && !boxColliderComponent->Empty();
        const auto& shapeConfig = entity.GetBox2DShapeConfig();
        const bool hasShapeConfig = shapeConfig && shapeConfig->type != Physics::Box2DShapeConfig::Type::None;
        const bool hasMeshData = entity.HasBox2DMesh() || !entity.GetBox2DBoundary().empty();
        if (!hasBoxColliderComponent && !hasShapeConfig && !hasMeshData)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Cannot register physics for tag='" << entity.GetTag()
                << "': collider component, shape config, and mesh data are all missing.\n";
            DebugTrace(oss.str());
            return;
        }

        const auto insertResult = m_registeredPhysicsBodies.insert(&entity);
        if (!insertResult.second)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Skipping duplicate physics registration for tag='" << entity.GetTag() << "'\n";
            DebugTrace(oss.str());
            return;
        }

        entity.AttachPhysicsMachine(m_physicsMachine);

        {
            bool hasBody = entity.HasBox2DBody();
            int fixtureCount = boxColliderComponent ? boxColliderComponent->createdFixtureCount : 0;
            if (!hasBody || fixtureCount <= 0)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Physics body incomplete for tag='" << entity.GetTag()
                    << "' (hasBody=" << std::boolalpha << hasBody
                    << ", fixtures=" << fixtureCount << "); attempting immediate rebuild.\n";
                ::DebugTrace(oss.str());

                LogImmediateDestroy(entity, "PhysicsRegistration::RegisterPhysicsBody (rebuild attempt)");
                entity.DetachPhysicsMachine();
                entity.AttachPhysicsMachine(m_physicsMachine);

                const bool rebuiltHasBody = entity.HasBox2DBody();
                const int rebuiltFixtures = boxColliderComponent ? boxColliderComponent->createdFixtureCount : 0;
                if (!rebuiltHasBody || rebuiltFixtures <= 0)
                {
                    std::ostringstream errorOss;
                    errorOss << kLogPrefix << "Failed to create physics body for tag='" << entity.GetTag()
                        << "' even after rebuild attempt (hasBody=" << std::boolalpha << rebuiltHasBody
                        << ", fixtures=" << rebuiltFixtures << ").\n";
                    ::DebugTrace(errorOss.str());

                    LogImmediateDestroy(entity, "PhysicsRegistration::RegisterPhysicsBody (rebuild failed)");
                    entity.DetachPhysicsMachine();
                    m_registeredPhysicsBodies.erase(&entity);
                    return;
                }

                hasBody = rebuiltHasBody;
                fixtureCount = rebuiltFixtures;

                std::ostringstream recovered;
                recovered << kLogPrefix << "Recovered physics body for tag='" << entity.GetTag()
                    << "' (fixtures=" << rebuiltFixtures << ").\n";
                ::DebugTrace(recovered.str());
            }

            float gravityScale = hasBody ? entity.GetGravityScale() : 0.0f;
            if (isPlayerEntity && hasBody && std::fabs(gravityScale - 0.0f) > std::numeric_limits<float>::epsilon())
            {
                entity.SetGravityScale(0.0f);
                gravityScale = entity.GetGravityScale();
                DebugTrace(std::string(kLogPrefix) + "Player gravity disabled to prevent unintended falling.\n");
            }

            std::ostringstream oss;
            oss << kLogPrefix << "Physics registration complete for tag='" << entity.GetTag()
                << "' hasBody=" << std::boolalpha << hasBody
                << " fixtures=" << fixtureCount
                << " gravityScale=" << gravityScale
                << ((hasBody && fixtureCount > 0) ? "" : " (incomplete)") << "\n";
            ::DebugTrace(oss.str());
        }
    }

    void PhysicsRegistration::UnregisterPhysicsBody(Entities::PhysicalEntity* entity)
    {
        if (!entity)
        {
            DebugTrace(std::string(kLogPrefix) + "UnregisterPhysicsBody received null entity pointer.\n");
            return;
        }

        const auto [it, inserted] = m_pendingUnregisters.insert(entity);
        if (inserted)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Unregister deferred until LateUpdate for tag='"
                << entity->GetTag() << "'.\n";
            DebugTrace(oss.str());
        }
    }

    void PhysicsRegistration::UnregisterPhysicsBodyImmediate(Entities::PhysicalEntity* entity)
    {
        if (!entity)
        {
            return;
        }

        const bool fixtureBuildFailed = entity->HasFixtureBuildFailure();

        const auto tracked = m_registeredPhysicsBodies.find(entity);
        if (tracked == m_registeredPhysicsBodies.end())
        {
            if (!fixtureBuildFailed)
            {
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << kLogPrefix << "UnregisterPhysicsBody called for untracked entity=" << entity << "\n";
                ::DebugTrace(oss.str());
#endif
            }
        }
        else
        {
            m_registeredPhysicsBodies.erase(tracked);
        }

        if (m_physicsMachine)
        {
            LogImmediateDestroy(*entity, "PhysicsRegistration::UnregisterPhysicsBody");
            entity->DetachPhysicsMachine();
        }
        else
        {
            if (!fixtureBuildFailed)
            {
#if defined(_DEBUG)
                DebugTrace(std::string(kLogPrefix) + "Physics machine missing during UnregisterPhysicsBody.\n");
#endif
            }
        }
    }

    void PhysicsRegistration::UnregisterAllPhysicsBodies()
    {
        if (m_registeredPhysicsBodies.empty())
        {
            return;
        }

        std::vector<Entities::PhysicalEntity*> bodies(m_registeredPhysicsBodies.begin(), m_registeredPhysicsBodies.end());
        for (Entities::PhysicalEntity* body : bodies)
        {
            UnregisterPhysicsBody(body);
        }
    }

    Core::IUpdatable* PhysicsRegistration::GetLateUpdateNode()
    {
        return m_lateUpdateNode.get();
    }

    const Core::IUpdatable* PhysicsRegistration::GetLateUpdateNode() const
    {
        return m_lateUpdateNode.get();
    }

    void PhysicsRegistration::ValidatePlayerPhysicsRegistration()
    {
        if (!m_playerEntity || !m_physicsMachine)
        {
            return;
        }

        const auto& physicsBodies = m_physicsMachine->GetBodies();
        const bool registeredWithPhysics = std::find(physicsBodies.begin(), physicsBodies.end(), m_playerEntity) != physicsBodies.end();
        if (!registeredWithPhysics)
        {
            RegisterPhysicsBody(*m_playerEntity);
            std::ostringstream oss;
            const auto* player = dynamic_cast<const Entities::Player*>(m_playerEntity);
            const bool isKinematic = player ? player->IsKinematic() : false;
            const bool isDynamic = player ? player->GetPhysical().IsDynamic() : m_playerEntity->GetPhysical().IsDynamic();
            oss << kLogPrefix << "Player body missing from PhysicsMachine list; re-registering (kinematic="
                << std::boolalpha << isKinematic
                << ", bodyType=" << (isDynamic ? "dynamic" : "static") << ").\n";
            ::DebugTrace(oss.str());
            m_loggedPlayerBodyRecovery = true;
        }
        else if (m_loggedPlayerBodyRecovery)
        {
            DebugTrace(std::string(kLogPrefix) + "Player body present in PhysicsMachine after prior recovery.\n");
            m_loggedPlayerBodyRecovery = false;
        }
    }

    void PhysicsRegistration::FlushPendingUnregisters()
    {
        if (m_pendingUnregisters.empty())
        {
            return;
        }

        std::vector<Entities::PhysicalEntity*> pending(m_pendingUnregisters.begin(), m_pendingUnregisters.end());
        m_pendingUnregisters.clear();

        for (Entities::PhysicalEntity* entity : pending)
        {
            UnregisterPhysicsBodyImmediate(entity);
        }
    }

    void PhysicsRegistration::AddContactHandler(ContactCallback onBegin, ContactCallback onEnd, ContactCallback onLost,
        ContactHitCallback onHit)
    {
        m_contactHandlers.push_back(ContactHandler{ std::move(onBegin), std::move(onEnd), std::move(onLost),
            std::move(onHit) });
    }

    void PhysicsRegistration::DispatchBeginContact(Entities::PhysicalEntity& a, Entities::PhysicalEntity& b)
    {
        for (const auto& handler : m_contactHandlers)
        {
            if (handler.onBegin)
            {
                handler.onBegin(a, b);
            }
        }
    }

    void PhysicsRegistration::DispatchEndContact(Entities::PhysicalEntity& a, Entities::PhysicalEntity& b)
    {
        for (const auto& handler : m_contactHandlers)
        {
            if (handler.onEnd)
            {
                handler.onEnd(a, b);
            }
        }
    }

    void PhysicsRegistration::DispatchLostContact(Entities::PhysicalEntity& a, Entities::PhysicalEntity& b)
    {
        for (const auto& handler : m_contactHandlers)
        {
            if (handler.onLost)
            {
                handler.onLost(a, b);
            }
        }
    }

    void PhysicsRegistration::DispatchHitContact(Entities::PhysicalEntity& a, Entities::PhysicalEntity& b, float approachSpeed)
    {
        for (const auto& handler : m_contactHandlers)
        {
            if (handler.onHit)
            {
                handler.onHit(a, b, approachSpeed);
            }
        }
    }
}
