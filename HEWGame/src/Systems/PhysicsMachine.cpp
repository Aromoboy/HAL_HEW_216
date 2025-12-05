#include "Systems/PhysicsMachine.h"
#include "Systems/IntegrateSystem.h"

#include <algorithm>
#include <utility>

#include "Core/IUpdatable.h"

namespace Systems
{
        PhysicsMachine::PhysicsMachine()
                : Core::UpdateNode(Core::UpdatePhase::Physics, 0)
                , m_system(nullptr)
                , m_integrator(std::make_shared<IntegrateSystem>())
                , m_bodies()
        {
        }

        void PhysicsMachine::SetSystem(SystemPtr system)
        {
                if (m_system == system)
                {
                        return;
                }

                m_system = std::move(system);
        }

        void PhysicsMachine::SetIntegrator(IntegratorPtr integrator)
        {
                if (m_integrator == integrator)
                {
                        return;
                }

                m_integrator = std::move(integrator);
        }

        PhysicsMachine::SystemPtr PhysicsMachine::GetSystem() const
        {
                return m_system;
        }

        PhysicsMachine::IntegratorPtr PhysicsMachine::GetIntegrator() const
        {
                return m_integrator;
        }

        const std::vector<Entities::PhysicalEntity*>& PhysicsMachine::GetBodies() const
        {
                return m_bodies;
        }

        void PhysicsMachine::RegisterBody(Entities::PhysicalEntity& body)
        {
                auto* bodyPtr = &body;
                if (std::find(m_bodies.begin(), m_bodies.end(), bodyPtr) != m_bodies.end())
                {
                        return;
                }

                m_bodies.push_back(bodyPtr);
        }

        void PhysicsMachine::UnregisterBody(Entities::PhysicalEntity& body)
        {
                auto* bodyPtr = &body;
                auto it = std::remove(m_bodies.begin(), m_bodies.end(), bodyPtr);
                if (it == m_bodies.end())
                {
                        return;
                }

                m_bodies.erase(it, m_bodies.end());
        }

        void PhysicsMachine::ClearBodies()
        {
                m_bodies.clear();
        }

        void PhysicsMachine::OnUpdate(Core::UpdateContext& ctx)
        {
                if (!m_system)
                {
                        return;
                }

                for (Entities::PhysicalEntity* body : m_bodies)
                {
                        if (!body)
                        {
                                continue;
                        }

                        m_system->Apply(*body, ctx.dt);

                        if (m_integrator)
                        {
                                m_integrator->Integrate(*body, ctx.dt);
                        }

                        body->SyncColliderBounds();
                }
        }
}

