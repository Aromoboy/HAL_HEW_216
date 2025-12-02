#include "Scene.h"

#include "Core/IUpdatable.h"
#include "Core/UpdateNode.h"
#include "Core/StepTimer.h"
#include "Core/Input.h"

#include <memory>
#include <sstream>
#include <utility>

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

Scene::Scene()
: m_isConfigured(false)
, m_input(nullptr)
, m_warnedAboutMissingCameraForDraw(false)
, m_sceneUpdateRegistered(false)
{
}

        Scene::~Scene()
        {
                UnregisterSceneUpdateNode();
        }

        void Scene::RootUpdate(Core::StepTimer& timer, Core::Input& input)
        {
                m_input = &input;

                if (!m_isConfigured)
                {
                        ConfigureUpdates();
                        m_isConfigured = true;
                }

                const float deltaTime = timer.DeltaTime();
                Core::UpdateContext ctx(deltaTime, timer, input, this);

                m_scheduler.Tick(ctx);
                m_cameraManager.Update();
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

        void Scene::Register(Core::IUpdatable& node)
        {
                m_scheduler.Register(node);
        }

        void Scene::Unregister(Core::IUpdatable& node)
        {
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
