#include "TitleScene.h"

#include "Core/Input.h"
#include "Core/StepTimer.h"

#include "PerspectiveCamera.h"
#include "SceneGame.h"
#include "ScenePowerUp.h"
#include "SettingScene.h"
#include "EffectTestScene.h"
#include "DirectX.h"

#include "BackgroundRenderer.h"
#include "TitleRenderer.h"
 
#include <memory>
#include <cmath>
#include <sstream>

TitleScene::TitleScene(SceneSpace::SceneManager& manager)
    : m_manager(&manager)
{
    for (auto& item : m_menu)
    {
        item.red = nullptr;
        item.green = nullptr;
    }
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

    // UI ボタン用テクスチャ読み込み
    auto loadButtonTexture = [](Texture& texture, const char* path, bool& valid)
        {
            const HRESULT hr = texture.Create(path);
            valid = SUCCEEDED(hr);
            if (FAILED(hr))
            {
                std::ostringstream oss;
                oss << "[TitleScene] Texture load failed: " << path << "\n";
                ::OutputDebugStringA(oss.str().c_str());
            }
        };

    loadButtonTexture(m_texStartRed, "Assets/Texture/start_red_btn.png", m_texStartRedValid);
    loadButtonTexture(m_texStartGreen, "Assets/Texture/start_green_btn.png", m_texStartGreenValid);
    loadButtonTexture(m_texPowerUpRed, "Assets/Texture/kyouka_red_btn.png", m_texPowerUpRedValid);
    loadButtonTexture(m_texPowerUpGreen, "Assets/Texture/kyouka_green_btn.png", m_texPowerUpGreenValid);
    loadButtonTexture(m_texExitRed, "Assets/Texture/exit_red_btn.png", m_texExitRedValid);
    loadButtonTexture(m_texExitGreen, "Assets/Texture/exit_green_btn.png", m_texExitGreenValid);

    DirectX::XMFLOAT4X4 proj;
    DirectX::XMStoreFloat4x4(
        &proj,
        DirectX::XMMatrixOrthographicOffCenterLH(0.0f, 1280.0f, 720.0f, 0.0f, 0.0f, 1.0f)
    );
    Sprite::SetProjection(proj);
   
}

void TitleScene::OnExit()
{
    if (m_background) m_background->Shutdown();
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

    // 更新処理（キー入力とカーソル操作）
    // 矢印上下でカーソル移動、決定でシーン切替
    float vertical = input->GetAxis(Core::ActionId::RotateYaw); 
    if (vertical > 0.5f)   
    {
        m_cursor--;
        if (m_cursor < 0) m_cursor = static_cast<int>(TitleMenu::Max) - 1;
    }
    else if (vertical < -0.5f)  
    {
        m_cursor++;
        if (m_cursor >= static_cast<int>(TitleMenu::Max)) m_cursor = 0;
    }

    // 決定
    if (input->IsTrigger(Core::ActionId::Confirm))
    {
        switch (static_cast<TitleMenu>(m_cursor))
        {
        case TitleMenu::Start:
            if (m_manager)
                m_manager->RequestSceneChange(std::make_unique<SceneGame>(*m_manager));
            break;
        case TitleMenu::PowerUp:
            if (m_manager)
                m_manager->RequestSceneChange(std::make_unique<SceneSpace::ScenePowerUp>(*m_manager));
            break;
        case TitleMenu::Exit:
            PostQuitMessage(0);
            break;
        }
       
    }

    if (input->IsTrigger('P'))
    {
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<SceneSpace::ScenePowerUp>(*m_manager));
        }
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

    if (input->IsTrigger(Core::ActionId::Cancel))
    {
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<TitleScene>(*m_manager));
        }
    }

}

void TitleScene::Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
{
    const SamplerState previousSampler = GetSamplerState();
    if (m_background) m_background->Draw();
    
    (void)activeCamera;

    auto drawUIButton = [](Texture& tex,
                           bool isValid,
                           const char* label,
                           float centerX,
                           float centerY,
                           float targetWidth,
                           float targetHeight)
        {
            if (!isValid)
            {
                std::ostringstream oss;
                oss << "[TitleScene] Skip UI texture (invalid): " << label << "\n";
                ::OutputDebugStringA(oss.str().c_str());
                return;
            }

            float texWidth = static_cast<float>(tex.GetWidth());
            float texHeight = static_cast<float>(tex.GetHeight());
            if (texWidth == 0.0f || texHeight == 0.0f)
            {
                std::ostringstream oss;
                oss << "[TitleScene] Skip UI texture (zero size): " << label
                    << " width=" << texWidth << " height=" << texHeight << "\n";
                ::OutputDebugStringA(oss.str().c_str());
                return;
            }

            const float snappedWidth = std::round(targetWidth);
            const float snappedHeight = std::round(targetHeight);
            const float snappedCenterX = std::round(centerX);
            const float snappedCenterY = std::round(centerY);

            DirectX::XMFLOAT4X4 worldFloat;
            DirectX::XMStoreFloat4x4(&worldFloat, DirectX::XMMatrixIdentity());

            Sprite::SetTexture(&tex);
            Sprite::SetWorld(worldFloat);
            Sprite::SetSize(DirectX::XMFLOAT2(snappedWidth, snappedHeight));
            Sprite::SetOffset(DirectX::XMFLOAT2(snappedCenterX, snappedCenterY));
            Sprite::Draw();
        };

    // --- ボタン配置 ---
    const float centerX = 640.0f;   // 画面中央
    const float screenHeight = 720.0f;
    const float spacing = 240.0f;   // ボタン間の縦間隔

    const float buttonWidth = 600.0f;
    const float buttonHeight = 220.0f;
    const float groupHeight = spacing * 2.0f + buttonHeight;
    const float startY = (screenHeight - groupHeight) * 0.5f + buttonHeight * 0.5f;

    // カーソルで色を切り替えて描画
    DirectX::XMFLOAT4X4 uiProjection;
    DirectX::XMFLOAT4X4 identity;
    DirectX::XMStoreFloat4x4(
        &uiProjection,
        DirectX::XMMatrixOrthographicOffCenterLH(0.0f, 1280.0f, 720.0f, 0.0f, 0.0f, 1.0f)
    );
    DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
    Sprite::SetProjection(uiProjection);
    Sprite::SetView(identity);
    Sprite::SetWorld(identity);
    Sprite::SetSize(DirectX::XMFLOAT2(1.0f, 1.0f));
    Sprite::SetOffset(DirectX::XMFLOAT2(0.0f, 0.0f));
    Sprite::SetColor(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    Sprite::SetUVPos(DirectX::XMFLOAT2(0.0f, 0.0f));
    Sprite::SetUVScale(DirectX::XMFLOAT2(1.0f, 1.0f));
    SetSamplerState(SAMPLER_POINT);
    drawUIButton(m_cursor == 0 ? m_texStartGreen : m_texStartRed,
                 m_cursor == 0 ? m_texStartGreenValid : m_texStartRedValid,
                 m_cursor == 0 ? "start_green_btn" : "start_red_btn",
                 centerX,
                 startY + spacing * 0,
                 buttonWidth,
                 buttonHeight);
    drawUIButton(m_cursor == 1 ? m_texPowerUpGreen : m_texPowerUpRed,
                 m_cursor == 1 ? m_texPowerUpGreenValid : m_texPowerUpRedValid,
                 m_cursor == 1 ? "kyouka_green_btn" : "kyouka_red_btn",
                 centerX,
                 startY + spacing * 1,
                 buttonWidth,
                 buttonHeight);
    drawUIButton(m_cursor == 2 ? m_texExitGreen : m_texExitRed,
                 m_cursor == 2 ? m_texExitGreenValid : m_texExitRedValid,
                 m_cursor == 2 ? "exit_green_btn" : "exit_red_btn",
                 centerX,
                 startY + spacing * 2,
                 buttonWidth,
                 buttonHeight);

    SetSamplerState(previousSampler);
}
