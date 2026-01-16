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

namespace
{
    constexpr const char* kLogPrefix = "[TitleScene] ";

    bool TryLoadTexture(Texture& texture, const char* path, const char* label)
    {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
        {
            std::ostringstream missing;
            missing << kLogPrefix << "Texture file missing for " << label << " ('" << path << "'). Using fallback.\n";
            Diagnostics::LogInfo(missing.str());

            const std::uint32_t fallbackPixel = 0xFFFFFFFFu;
            const HRESULT fallbackHr = texture.Create(DXGI_FORMAT_R8G8B8A8_UNORM, 1u, 1u, &fallbackPixel);
            if (SUCCEEDED(fallbackHr))
            {
                return true;
            }

            std::ostringstream fallbackLog;
            fallbackLog << kLogPrefix << "Failed to create fallback texture for " << label << ".\n";
            Diagnostics::LogInfo(fallbackLog.str());
            return false;
        }

        const HRESULT hr = texture.Create(path);
        if (SUCCEEDED(hr))
        {
            return true;
        }

        std::ostringstream oss;
        oss << kLogPrefix << "Failed to load " << label << " texture '" << path << "'. Using fallback.\n";
        Diagnostics::LogInfo(oss.str());

        const std::uint32_t fallbackPixel = 0xFFFFFFFFu;
        const HRESULT fallbackHr = texture.Create(DXGI_FORMAT_R8G8B8A8_UNORM, 1u, 1u, &fallbackPixel);
        if (SUCCEEDED(fallbackHr))
        {
            return true;
        }

        std::ostringstream fallbackLog;
        fallbackLog << kLogPrefix << "Failed to create fallback texture for " << label << ".\n";
        Diagnostics::LogInfo(fallbackLog.str());
        return false;
    }
}

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
    BackgroundRendererConfig::BackgroundLayer layer;
    layer.texturePath = "Assets/Texture/StartScreen.png";
    layer.parallaxFactor = 0.0f;
    layer.samplingMode = BackgroundRendererConfig::SamplingMode::Clamp;
    layer.tintColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    cfg.layers.push_back(layer);
    if (!cfg.layers.empty())
    {
        m_background = std::make_unique<BackgroundRenderer>();
        if (!m_background->Initialize(cfg))
        {
            m_background.reset();
        }
    }

    // UI ボタン用テクスチャ読み込み
    const bool startRedOk = TryLoadTexture(m_texStartRed, "Assets/Texture/タイトル画面UI5.png", "Start (red)");
    const bool startGreenOk = TryLoadTexture(m_texStartGreen, "Assets/Texture/タイトル画面UI6.png", "Start (green)");
    m_startButtonEnabled = startRedOk && startGreenOk;
    if (!m_startButtonEnabled)
    {
        Diagnostics::LogInfo(std::string(kLogPrefix) + "Start button disabled due to missing textures.\n");
    }

    const bool exitRedOk = TryLoadTexture(m_texExitRed, "Assets/Texture/タイトル画面UI1.png", "Exit (red)");
    const bool exitGreenOk = TryLoadTexture(m_texExitGreen, "Assets/Texture/タイトル画面UI2.png", "Exit (green)");
    m_exitButtonEnabled = exitRedOk && exitGreenOk;
    if (!m_exitButtonEnabled)
    {
        Diagnostics::LogInfo(std::string(kLogPrefix) + "Exit button disabled due to missing textures.\n");
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
            if (m_manager && m_startButtonEnabled)
                m_manager->RequestSceneChange(std::make_unique<SceneGame>(*m_manager));
            break;
        case TitleMenu::Exit:
            if (m_exitButtonEnabled)
            {
                PostQuitMessage(0);
            }
            break;
        }
       
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
    if (m_background) m_background->Draw(0.0f, 1.0f);
    
    (void)activeCamera;

    // エラー出ているのでコメントアウトした　ドロスより
    /*auto drawUIButton = [](Texture& tex, float centerX, float centerY, float targetWidth, float targetHeight)
        {
            float texWidth = static_cast<float>(tex.GetWidth());
            float texHeight = static_cast<float>(tex.GetHeight());

            // 元画像比率に合わせてスケール
            float scaleX = targetWidth / texWidth;
            float scaleY = targetHeight / texHeight;

            // 原点をテクスチャ中心に移動
            DirectX::XMMATRIX centering = DirectX::XMMatrixTranslation(-texWidth * 0.5f, -texHeight * 0.5f, 0.0f);

            // スケーリング
            DirectX::XMMATRIX scaling = DirectX::XMMatrixScaling(scaleX, scaleY, 1.0f);

            // 画面上の中心位置に移動
            DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(centerX, centerY, 0.0f);

            // 掛け順: 画面移動 * スケーリング * 原点補正
            DirectX::XMMATRIX world = translation * scaling * centering;

            DirectX::XMFLOAT4X4 worldFloat;
            DirectX::XMStoreFloat4x4(&worldFloat, world);

            Sprite::SetTexture(&tex);
            Sprite::SetWorld(worldFloat);
            Sprite::Draw();
        };

    // --- ボタン配置 ---
    const float centerX = 640.0f;   // 画面中央
    const float startY = 250.0f;    // 最初のボタン 
    const float spacing = 240.0f;   // ボタン間の縦間隔

    const float buttonWidth = 600.0f;  
    const float buttonHeight = 220.0f;  

    // カーソルで色を切り替えて描画
    drawUIButton(m_cursor == 0 ? m_texStartGreen : m_texStartRed, centerX, startY + spacing * 0, buttonWidth, buttonHeight);
    drawUIButton(m_cursor == 1 ? m_texExitGreen : m_texExitRed, centerX, startY + spacing * 1, buttonWidth, buttonHeight);*/
    
}
