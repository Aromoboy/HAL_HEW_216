#include "Scene.h"

#include "Core/IUpdatable.h"
#include "Core/UpdateNode.h"
#include "Core/StepTimer.h"
#include "Core/Input.h"
#include "Debug.h"
#include "Render/DirectX.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <sstream>
#include <utility>

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

        void OnUpdate(const Core::UpdateContext& ctx) override
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

        void OnUpdate(const Core::UpdateContext& ctx) override
        {
                m_owner.m_cameraManager.Update(ctx);
        }

private:
        Scene& m_owner;
};

Scene::Scene()
: m_isConfigured(false)
, m_input(nullptr)
, m_timer(nullptr)
, m_sceneUpdateRegistered(false)
, m_cameraUpdateRegistered(false)
{
}

        Scene::~Scene()
        {
                UnregisterSceneUpdateNode();
                UnregisterCameraUpdateNode();
        }

        void Scene::RootUpdate(Core::StepTimer& timer, Core::Input& input)
        {
                m_input = &input;
                m_timer = &timer;

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
                float clampedDeltaTime = deltaTime;
                if (!m_initialFrameProcessed)
                {
                        if (m_initialFrameClampMaxDt > 0.0f
                            && std::isfinite(deltaTime)
                            && deltaTime > m_initialFrameClampMaxDt)
                        {
                                clampedDeltaTime = m_initialFrameClampMaxDt;
                                std::ostringstream oss;
                                oss << "[Scene] First frame dt clamped: dt=" << deltaTime
                                    << " -> " << clampedDeltaTime
                                    << " (maxStepDt=" << m_initialFrameClampMaxDt << ")\n";
                                DebugLog::DebugMessage(oss.str());
                        }

                        m_initialFrameProcessed = true;
                }

                Core::UpdateContext ctx(clampedDeltaTime, timer);

                m_scheduler.Tick(ctx);
        }

void Scene::RootDraw()
{
        auto activeCamera = m_cameraManager.GetActiveCamera();

        DrawWorld(activeCamera);
        RenderTarget* defaultRTV = GetDefaultRTV();
        SetRenderTargets(1, &defaultRTV, nullptr);
        DrawOverlay();
}

        void Scene::ConfigureUpdates()
        {
        }

        void Scene::DrawOverlay()
        {
        }

        void Scene::DrawWorld(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
        {
                Draw(activeCamera);
        }

        void Scene::ConfigureInitialFrameClamp(float maxStepDt)
        {
                m_initialFrameClampMaxDt = std::max(0.0f, maxStepDt);
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
                        m_sceneUpdateRegistered = Register(*m_sceneUpdateNode);
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
                        m_cameraUpdateRegistered = Register(*m_cameraUpdateNode);
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

        bool Scene::Register(Core::IUpdatable& node)
        {
                auto handle = m_scheduler.Register(node);
                if (!handle.IsValid())
                {
                        DebugLog::DebugMessage("[Scene] Register failed; scheduler returned invalid handle.\n");
                        assert(false && "Scene::Register failed.");
                        return false;
                }

                if (auto* updateNode = dynamic_cast<Core::UpdateNode*>(&node))
                {
                        updateNode->SetRegistrationHandle(std::move(handle));
                }
                else
                {
                        handle.Release();
                }
                return true;
        }

        void Scene::Unregister(Core::IUpdatable& node)
        {
                if (auto* updateNode = dynamic_cast<Core::UpdateNode*>(&node))
                {
                        updateNode->ReleaseRegistrationHandle();
                }
                else
                {
                        m_scheduler.Unregister(node);
                }
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

        Core::StepTimer* Scene::GetTimer()
        {
                return m_timer;
        }

        const Core::StepTimer* Scene::GetTimer() const
        {
                return m_timer;
        }

CameraSpace::CameraManager& Scene::GetCameraManager()
{
return m_cameraManager;
}

const CameraSpace::CameraManager& Scene::GetCameraManager() const
{
return m_cameraManager;
}

std::shared_ptr<::CameraSpace::ICamera> Scene::AddCamera(std::shared_ptr<::CameraSpace::ICamera> camera)
{
    return m_cameraManager.AddCamera(std::move(camera));
}

void Scene::SetActiveCamera(const std::shared_ptr<::CameraSpace::ICamera>& camera)
{
    m_cameraManager.SetActive(camera);
}

std::shared_ptr<::CameraSpace::ICamera> Scene::GetActiveCamera() const
{
    return m_cameraManager.GetActiveCamera();
}
}
