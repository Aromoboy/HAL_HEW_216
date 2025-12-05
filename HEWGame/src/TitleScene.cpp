#include "TitleScene.h"

#include "Core/Input.h"
#include "Core/StepTimer.h"
#include "PerspectiveCamera.h"
#include "SceneGame.h"

#include <memory>

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
        return;
    }

    SetActiveCamera(registeredCamera);
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

void TitleScene::Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera)
{
    (void)activeCamera;
}
