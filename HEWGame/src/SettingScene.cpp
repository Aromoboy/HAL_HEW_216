#include "SettingScene.h"

#include "Core/Input.h"
#include "Core/StepTimer.h"

#include "PerspectiveCamera.h"
#include "SceneGame.h"
#include "TitleScene.h"

#include "BackgroundRenderer.h"
#include "TitleRenderer.h"

SceneSpace::SettingScene::SettingScene(SceneSpace::SceneManager& manager)
    : m_manager(&manager)
{
}

bool SceneSpace::SettingScene::Load(Core::Input& input)
{
	(void)input;
    return true;
}


void SceneSpace::SettingScene::Update(Core::StepTimer& timer, Core::Input& input)
{
    
	RootUpdate(timer, input);
}

void SceneSpace::SettingScene::Draw()
{
	RootDraw();
}

void SceneSpace::SettingScene::OnEnter()
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

void SceneSpace::SettingScene::OnExit()
{
    if (m_background) m_background->Shutdown();
    m_background.reset();
    UnregisterSceneUpdateNode();
}

void SceneSpace::SettingScene::ConfigureUpdates()
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

void SceneSpace::SettingScene::Update(const Core::UpdateContext& ctx)
{
    (void)ctx;
    Core::Input* input = GetInput();
    if (input && input->IsTrigger(Core::ActionId::Cancel))//CancelESCkeyŐݒʂ^Cgʂɖ߂
    {
        if (m_manager)
        {
            m_manager->RequestSceneChange(
                std::make_unique<TitleScene>(*m_manager)
            );
        }
    }
}

void SceneSpace::SettingScene::Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
{
 
    (void)activeCamera;
}
