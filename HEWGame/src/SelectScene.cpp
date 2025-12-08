#include "SelectScene.h"

#include "Core/Input.h"
#include "Core/StepTimer.h"

SelectScene::SelectScene() = default;

void SelectScene::Load(Core::Input& input)
{
    (void)input;
}

void SelectScene::Update(Core::StepTimer& timer, Core::Input& input)
{
    RootUpdate(timer, input);
}

void SelectScene::Draw()
{
    RootDraw();
}

void SelectScene::OnEnter()
{
}

void SelectScene::OnExit()
{
}

void SelectScene::ConfigureUpdates()
{
    if (auto* input = GetInput())
    {
        Register(*input);
    }

    RegisterSceneUpdateNode(Core::UpdatePhase::Gameplay, 0);
}

void SelectScene::Update(Core::UpdateContext& ctx)
{
    (void)ctx;
}

void SelectScene::Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
{
    (void)activeCamera;
}
