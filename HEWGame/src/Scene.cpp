// ============================================================================
// 概要: Scene の挙動や関連ロジックを実装し、src に属する機能を具体化している。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: src 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#include "Scene.h"

#include "Core/IUpdatable.h"
#include "Core/StepTimer.h"
#include "Core/Input.h"

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
Scene::Scene()
: m_isConfigured(false)
, m_input(nullptr)
, m_warnedAboutMissingCameraForDraw(false)
        {
        }

        Scene::~Scene()
        {
        }

        void Scene::RootUpdate(Core::StepTimer& timer, Core::Input& input)
        {
                m_input = &input;

                if (!m_isConfigured)
                {
                        ConfigureUpdates();
                        m_isConfigured = true;
                }

                const float deltaTime = timer.DeltaSeconds();
                Core::UpdateContext ctx(deltaTime, timer, input, this);

                Update(ctx);
                m_cameraManager.Update();
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
