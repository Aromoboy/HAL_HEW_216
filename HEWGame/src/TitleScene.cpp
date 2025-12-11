#include "TitleScene.h"

#include "Core/Input.h"
#include "Core/StepTimer.h"
#include "PerspectiveCamera.h"
#include "SceneGame.h"

#include <memory>
#include <sstream>

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

        constexpr const char* kLogPrefix = "[TitleScene] ";
}

TitleScene::TitleScene(SceneSpace::SceneManager& manager)
    : m_manager(&manager)
{
}

void TitleScene::Load(Core::Input& input)
{
    (void)input;
}

void TitleScene::Update(Core::StepTimer& timer, Core::Input& input)
{
    RootUpdate(timer, input);
}

void TitleScene::Draw()
{
    RootDraw();
}

void TitleScene::OnEnter()
{
    if (!m_camera)
    {
        m_camera = std::make_shared<CameraSpace::PerspectiveCamera>();
    }

    auto registeredCamera = AddCamera(m_camera);
    if (!registeredCamera)
    {
        DebugTrace(std::string(kLogPrefix) + "Failed to register title camera.\n");
        return;
    }

    SetActiveCamera(registeredCamera);

    if (!GetActiveCamera())
    {
        DebugTrace(std::string(kLogPrefix) + "SetActiveCamera failed; no active camera after registration.\n");
    }
}

void TitleScene::OnExit()
{
    UnregisterSceneUpdateNode();
}

void TitleScene::ConfigureUpdates()
{
    if (auto* input = GetInput())
    {
        Register(*input);
    }

    RegisterSceneUpdateNode(Core::UpdatePhase::Gameplay, 0);
}

void TitleScene::Update(Core::UpdateContext& ctx)
{
    if (ctx.input.IsTrigger(Core::ActionId::Confirm))
    {
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<SceneGame>());
        }
    }
}

void TitleScene::Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
{
    (void)activeCamera;
}
