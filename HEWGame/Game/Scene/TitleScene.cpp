#include "TitleScene.h"

#include "Core/Input.h"
#include "Core/StepTimer.h"

#include "PerspectiveCamera.h"
#include "SceneGame.h"
#include "SettingScene.h"
#include "EffectTestScene.h"
#include "DirectX.h"

#include "BackgroundRenderer.h"
#include "TitleRenderer.h"
#include "Diagnostics/Logger.h"
 
#include <cstdint>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>

TitleScene::TitleScene(SceneSpace::SceneManager& manager)
    : m_manager(&manager)
{
}

bool TitleScene::Load(Core::Input& input)
{
    (void)input;
    return true;
}

void TitleScene::Update(Core::StepTimer& timer, Core::Input& input)
{
    RootUpdate(timer, input);
}

void TitleScene::Draw()
{
    RootDraw();
}

void TitleScene::Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
{
    if (!m_background)
    {
        (void)activeCamera;
        return;
    }

    if (!activeCamera)
    {
        return;
    }

    m_background->Draw(0.0f, 1.0f);
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
    BackgroundRendererConfig::BackgroundLayer layer;
    layer.texturePath = "Assets/Texture/StartScreen.png";
    layer.parallaxFactor = 0.0f;
    layer.samplingMode = BackgroundRendererConfig::SamplingMode::Clamp;
    layer.tintColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    cfg.layers.push_back(layer);
    m_background = std::make_unique<BackgroundRenderer>();
    if (!m_background->Initialize(cfg))
    {
        Diagnostics::LogInfo("[TitleScene] Background renderer failed to initialize.");
        m_background.reset();
    }

    DirectX::XMFLOAT4X4 proj;
    DirectX::XMStoreFloat4x4(
        &proj,
        DirectX::XMMatrixOrthographicOffCenterLH(0.0f, 1280.0f, 720.0f, 0.0f, 0.0f, 1.0f)
    );
    Sprite::SetProjection(proj);
   
}

void TitleScene::OnExit()
{
    if (m_background)
    {
        m_background->Shutdown();
    }
    m_background.reset();
    if (m_inputRegistered)
    {
        if (auto* input = GetInput())
        {
            Unregister(*input);
        }
        m_inputRegistered = false;
    }
    UnregisterSceneUpdateNode();
}

void TitleScene::ConfigureUpdates()
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

void TitleScene::Update(const Core::UpdateContext& ctx)
{
    (void)ctx;
    Core::Input* input = GetInput();
    if (!input)
    {
        return;
    }


    if (input->IsTrigger(VK_F1))
    {
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<EffectTestScene>(*m_manager));
        }
        return;
    }

    if (input->IsTrigger(VK_SPACE))
    {
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<SceneSpace::SettingScene>(*m_manager));
        }
        return;
    }

    if (input->IsTrigger(Core::ActionId::Confirm))
    {
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<SceneGame>(*m_manager));
        }
        return;
    }

    if (input->IsTrigger(Core::ActionId::Cancel))
    {
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<TitleScene>(*m_manager));
        }
    }

}
