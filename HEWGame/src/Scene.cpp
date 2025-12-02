#include "Scene.h"

#include "Core/IUpdatable.h"
#include "Core/UpdateNode.h"
#include "Core/StepTimer.h"
#include "Core/Input.h"

#include <algorithm>
#include <array>
#include <memory>
#include <sstream>
#include <typeinfo>
#include <utility>
#include <vector>

#if defined(_WIN32)
#       ifndef NOMINMAX
#               define NOMINMAX
#       endif
#       include <windows.h>
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
}

namespace SceneSpace
{
class Scene::SceneUpdateNode : public Core::UpdateNode
{
public:
        SceneUpdateNode(Scene& owner, Core::UpdatePhase phase, int priority)
                : Core::UpdateNode(phase, priority)
                , m_owner(owner)
        {
        }

        void OnUpdate(Core::UpdateContext& ctx) override
        {
                m_owner.Update(ctx);
        }

private:
        Scene& m_owner;
};

class Scene::CameraUpdateNode : public Core::UpdateNode
{
public:
        CameraUpdateNode(Scene& owner, Core::UpdatePhase phase, int priority)
                : Core::UpdateNode(phase, priority)
                , m_owner(owner)
        {
        }

        void OnUpdate(Core::UpdateContext& ctx) override
        {
                m_owner.m_cameraManager.Update(ctx);
        }

private:
        Scene& m_owner;
};

Scene::Scene()
: m_isConfigured(false)
, m_input(nullptr)
, m_warnedAboutMissingCameraForDraw(false)
, m_loggedExecutionPlan(false)
, m_lastExecutionLogSize(0)
, m_sceneUpdateRegistered(false)
, m_cameraUpdateRegistered(false)
{
#if defined(_DEBUG)
        m_scheduler.SetExecutionLogListener([this](const Core::UpdateScheduler::ExecutionLog& log)
        {
                LogExecutionPlan(log);
        });
#endif
}

        Scene::~Scene()
        {
                UnregisterSceneUpdateNode();
                UnregisterCameraUpdateNode();
        }

        void Scene::RootUpdate(Core::StepTimer& timer, Core::Input& input)
        {
                m_input = &input;

                if (!m_isConfigured)
                {
                        ConfigureUpdates();
                        m_isConfigured = true;
                }

                if (!m_cameraUpdateRegistered)
                {
                        RegisterCameraUpdateNode(Core::UpdatePhase::Camera, 0);
                }

                const float deltaTime = timer.DeltaTime();
                Core::UpdateContext ctx(deltaTime, timer, input, this);

                m_scheduler.Tick(ctx);
        }

void Scene::RootDraw()
{
        auto activeCamera = m_cameraManager.GetActiveCamera();
        if (!activeCamera)
        {
#if defined(_DEBUG)
                if (!m_warnedAboutMissingCameraForDraw)
                {
                        std::ostringstream oss;
                        oss << "[Scene] RootDraw detected missing active camera in scene ptr=" << this << '\n';
                        DebugTrace(oss.str());
                        m_warnedAboutMissingCameraForDraw = true;
                }
#endif
        }
        else
        {
                m_warnedAboutMissingCameraForDraw = false;
        }

        Draw(activeCamera);
}

        void Scene::ConfigureUpdates()
        {
        }

        void Scene::RegisterSceneUpdateNode(Core::UpdatePhase phase, int priority)
        {
                if (!m_sceneUpdateNode)
                {
                        m_sceneUpdateNode = std::make_unique<SceneUpdateNode>(*this, phase, priority);
                }
                else
                {
                        m_sceneUpdateNode->SetPhase(phase);
                        m_sceneUpdateNode->SetPriority(priority);
                }

                if (!m_sceneUpdateRegistered)
                {
                        Register(*m_sceneUpdateNode);
                        m_sceneUpdateRegistered = true;
                }
        }

        void Scene::UnregisterSceneUpdateNode()
        {
                if (m_sceneUpdateRegistered && m_sceneUpdateNode)
                {
                        Unregister(*m_sceneUpdateNode);
                        m_sceneUpdateRegistered = false;
                }
        }

        Core::IUpdatable* Scene::GetSceneUpdateNode()
        {
                return m_sceneUpdateNode.get();
        }

        const Core::IUpdatable* Scene::GetSceneUpdateNode() const
        {
                return m_sceneUpdateNode.get();
        }

        void Scene::RegisterCameraUpdateNode(Core::UpdatePhase phase, int priority)
        {
                if (!m_cameraUpdateNode)
                {
                        m_cameraUpdateNode = std::make_unique<CameraUpdateNode>(*this, phase, priority);
                }
                else
                {
                        m_cameraUpdateNode->SetPhase(phase);
                        m_cameraUpdateNode->SetPriority(priority);
                }

                if (!m_cameraUpdateRegistered)
                {
                        Register(*m_cameraUpdateNode);
                        m_cameraUpdateRegistered = true;
                }
        }

        void Scene::UnregisterCameraUpdateNode()
        {
                if (m_cameraUpdateRegistered && m_cameraUpdateNode)
                {
                        Unregister(*m_cameraUpdateNode);
                        m_cameraUpdateRegistered = false;
                }
        }

        Core::IUpdatable* Scene::GetCameraUpdateNode()
        {
                return m_cameraUpdateNode.get();
        }

        const Core::IUpdatable* Scene::GetCameraUpdateNode() const
        {
                return m_cameraUpdateNode.get();
        }

        void Scene::LogExecutionPlan(const Core::UpdateScheduler::ExecutionLog& log)
        {
#if defined(_DEBUG)
                if (log.empty())
                {
                        return;
                }

                const bool sizeChanged = log.size() != m_lastExecutionLogSize;
                if (m_loggedExecutionPlan && !sizeChanged)
                {
                        return;
                }

                m_lastExecutionLogSize = log.size();
                m_loggedExecutionPlan = true;

                constexpr std::size_t kPhaseCount = static_cast<std::size_t>(Core::UpdatePhase::Late) + 1;
                std::array<bool, kPhaseCount> phaseSeen{};

                auto ordered = log;
                std::sort(ordered.begin(), ordered.end(),
                        [](const Core::UpdateScheduler::ExecutionRecord& lhs, const Core::UpdateScheduler::ExecutionRecord& rhs)
                        {
                                if (lhs.phase != rhs.phase)
                                {
                                        return lhs.phase < rhs.phase;
                                }
                                if (lhs.priority != rhs.priority)
                                {
                                        return lhs.priority < rhs.priority;
                                }
                                if (lhs.registrationOrder != rhs.registrationOrder)
                                {
                                        return lhs.registrationOrder < rhs.registrationOrder;
                                }
                                return lhs.executionIndex < rhs.executionIndex;
                        });

                std::ostringstream oss;
                oss << "[Scene] UpdateScheduler execution plan scene=" << this
                        << " nodeCount=" << ordered.size() << "\n";

                for (const auto& record : ordered)
                {
                        const std::size_t phaseIndex = static_cast<std::size_t>(record.phase);
                        if (phaseIndex < phaseSeen.size())
                        {
                                phaseSeen[phaseIndex] = true;
                        }

                        oss << "  phase=" << Core::ToString(record.phase)
                                << '(' << static_cast<int>(record.phase) << ')'
                                << " priority=" << record.priority
                                << " order=" << record.registrationOrder
                                << " execIndex=" << record.executionIndex
                                << " invoked=" << (record.invoked ? 1 : 0)
                                << " node=" << record.node;

#if defined(_CPPRTTI)
                        if (record.node)
                        {
                                oss << " type=" << typeid(*record.node).name();
                        }
#endif

                        oss << '\n';
                }

                std::vector<Core::UpdatePhase> missing;
                for (std::size_t i = 0; i < phaseSeen.size(); ++i)
                {
                        if (!phaseSeen[i])
                        {
                                missing.emplace_back(static_cast<Core::UpdatePhase>(i));
                        }
                }

                if (!missing.empty())
                {
                        oss << "  missing phases:";
                        for (Core::UpdatePhase phase : missing)
                        {
                                oss << ' ' << Core::ToString(phase)
                                        << '(' << static_cast<int>(phase) << ')';
                        }
                        oss << '\n';
                }

                DebugTrace(oss.str());
#else
                (void)log;
#endif
        }

        void Scene::Register(Core::IUpdatable& node)
        {
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << "[Scene] Register requested scene=" << this
                        << " node=" << &node
                        << " phase=" << Core::ToString(node.GetPhase())
                        << '(' << static_cast<int>(node.GetPhase()) << ')'
                        << " priority=" << node.GetPriority() << '\n';
                DebugTrace(oss.str());
#endif
                m_loggedExecutionPlan = false;
                m_scheduler.Register(node);
        }

        void Scene::Unregister(Core::IUpdatable& node)
        {
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << "[Scene] Unregister requested scene=" << this
                        << " node=" << &node
                        << " phase=" << Core::ToString(node.GetPhase())
                        << '(' << static_cast<int>(node.GetPhase()) << ')'
                        << " priority=" << node.GetPriority() << '\n';
                DebugTrace(oss.str());
#endif
                m_loggedExecutionPlan = false;
                m_scheduler.Unregister(node);
        }

        Core::UpdateScheduler& Scene::GetUpdateScheduler()
        {
                return m_scheduler;
        }

        Core::Input* Scene::GetInput()
        {
                return m_input;
        }

        const Core::Input* Scene::GetInput() const
        {
                return m_input;
        }

CameraSpace::CameraManager& Scene::GetCameraManager()
{
return m_cameraManager;
}

const CameraSpace::CameraManager& Scene::GetCameraManager() const
{
return m_cameraManager;
}

std::shared_ptr<CameraSpace::ICamera> Scene::AddCamera(std::shared_ptr<CameraSpace::ICamera> camera)
{
return m_cameraManager.AddCamera(std::move(camera));
}

void Scene::SetActiveCamera(const std::shared_ptr<CameraSpace::ICamera>& camera)
{
m_cameraManager.SetActive(camera);
}

std::shared_ptr<CameraSpace::ICamera> Scene::GetActiveCamera() const
{
return m_cameraManager.GetActiveCamera();
}
}
