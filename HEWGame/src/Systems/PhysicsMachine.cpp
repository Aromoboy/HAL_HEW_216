#include "Systems/PhysicsMachine.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>

#if defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

#include "Core/IUpdatable.h"

namespace
{
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
}

namespace Systems
{
        PhysicsMachine::PhysicsMachine()
                : Core::UpdateNode(Core::UpdatePhase::Physics, 0)
                , m_system(nullptr)
                , m_bodies()
                , m_loggedMissingSystem(false)
        {
        }

        void PhysicsMachine::SetSystem(SystemPtr system)
        {
                if (m_system == system)
                {
                        return;
                }

#if defined(_DEBUG)
                {
                        std::ostringstream oss;
                        oss << "[PhysicsMachine] SetSystem called. previous=" << m_system.get()
                            << " new=" << (system ? system.get() : nullptr) << "\n";
                        DebugTrace(oss.str());
                }
#endif

                m_system = std::move(system);
                m_loggedMissingSystem = false;
        }

        PhysicsMachine::SystemPtr PhysicsMachine::GetSystem() const
        {
                return m_system;
        }

        const std::vector<SceneSpace::PhysicalEntity*>& PhysicsMachine::GetBodies() const
        {
                return m_bodies;
        }

        void PhysicsMachine::RegisterBody(SceneSpace::PhysicalEntity& body)
        {
                auto* bodyPtr = &body;
                if (std::find(m_bodies.begin(), m_bodies.end(), bodyPtr) != m_bodies.end())
                {
#if defined(_DEBUG)
                        std::ostringstream oss;
                        oss << "[PhysicsMachine] Duplicate body registration detected body=" << bodyPtr << "\n";
                        DebugTrace(oss.str());
#endif
                        return;
                }

                m_bodies.push_back(bodyPtr);
        }

        void PhysicsMachine::UnregisterBody(SceneSpace::PhysicalEntity& body)
        {
                auto* bodyPtr = &body;
                auto it = std::remove(m_bodies.begin(), m_bodies.end(), bodyPtr);
                if (it == m_bodies.end())
                {
#if defined(_DEBUG)
                        std::ostringstream oss;
                        oss << "[PhysicsMachine] Unregister requested for unknown body body=" << bodyPtr << "\n";
                        DebugTrace(oss.str());
#endif
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
                        if (!m_bodies.empty() && !m_loggedMissingSystem)
                        {
                                std::ostringstream oss;
                                oss << "[PhysicsMachine] No physics system configured while bodies are registered. bodyCount="
                                    << m_bodies.size() << "\n";
                                DebugTrace(oss.str());
                                m_loggedMissingSystem = true;
                        }
                        return;
                }

                for (SceneSpace::PhysicalEntity* body : m_bodies)
                {
                        if (!body)
                        {
#if defined(_DEBUG)
                                DebugTrace("[PhysicsMachine] Encountered null body pointer during update.\n");
#endif
                                continue;
                        }

                        m_system->Apply(*body, ctx.dt);
                }
        }
}

