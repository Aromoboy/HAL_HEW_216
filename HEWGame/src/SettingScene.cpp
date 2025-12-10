#include "SettingScene.h"
#include "Core/Input.h"
#include "Core/StepTimer.h"

#include "PerspectiveCamera.h"
#include "SceneGame.h"



void SceneSpace::SettingScene::Load(Core::Input& input)
{
	(void)input;
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
    if (auto* input = GetInput())
    {
        Register(*input);
    }

    RegisterSceneUpdateNode(Core::UpdatePhase::Gameplay, 0);
}

void SceneSpace::SettingScene::Update(Core::UpdateContext& ctx)
{
    if (ctx.input.IsTrigger(Core::ActionId::Confirm))
    {
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<SceneGame>());
        }
    }
}

void SceneSpace::SettingScene::Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
{
 

    (void)activeCamera;
}
