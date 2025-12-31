#include "SelectScene.h"

#include "Core/Input.h"
#include "Core/StepTimer.h"

SelectScene::SelectScene() = default;

bool SelectScene::Load(Core::Input& input)
{
    (void)input;
    return true;
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
    if (m_inputRegistered)
    {
        if (auto* input = GetInput())
        {
            Unregister(*input);
        }
        m_inputRegistered = false;
    }
}

void SelectScene::ConfigureUpdates()
{
    if (!m_inputRegistered)
    {
        if (auto* input = GetInput())
        {
            if (Register(*input))
            {
                m_inputRegistered = true;
            }
        }
    }

    RegisterSceneUpdateNode(Core::UpdatePhase::Gameplay, 0);
}

void SelectScene::Update(const Core::UpdateContext& ctx)
{
    (void)ctx;
}

void SelectScene::Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
{
    (void)activeCamera;
}
