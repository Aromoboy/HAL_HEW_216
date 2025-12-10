#include "TitleScene.h"

#include "Core/Input.h"
#include "Core/StepTimer.h"

#include "PerspectiveCamera.h"
#include "SceneGame.h"
#include "SettingScene.h"

#include "BackgroundRenderer.h"
#include "TitleRenderer.h"

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

    BackgroundRendererConfig cfg{};
    cfg.texturePath = "Assets/Texture/StartScreen.png";
    m_background = std::make_unique<BackgroundRenderer>();
    if (!m_background->Initialize(cfg)) 
    {
        m_background.reset();
    }

}

void TitleScene::OnExit()
{
    if (m_background) m_background->Shutdown();
    m_background.reset();
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
    if (ctx.input.IsTrigger(Core::ActionId::Confirm))//Corfirm ← Enterkeyで、タイトルシーンからゲームシーンに入る
    {
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<SceneGame>());
        }
    }
    

    if (ctx.input.IsTrigger(Core::ActionId::Pause))  //Pause ← spacekeyで、タイトルシーンから設定シーンに入る
    {
        if (m_manager)
        {
            m_manager->RequestSceneChange( std::make_unique<SceneSpace::SettingScene>(*m_manager)
            );
        }
    }

}

void TitleScene::Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
{
    if (m_background) m_background->Draw();
    
    (void)activeCamera;
}
