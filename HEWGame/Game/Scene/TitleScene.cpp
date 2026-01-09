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
    m_texStartRed.Create("Assets/Texture/タイトル画面UI5.png");
    m_texStartGreen.Create("Assets/Texture/タイトル画面UI6.png");
    m_texPowerUpRed.Create("Assets/Texture/タイトル画面UI3.png");
    m_texPowerUpGreen.Create("Assets/Texture/タイトル画面UI4.png");
    m_texExitRed.Create("Assets/Texture/タイトル画面UI1.png");
    m_texExitGreen.Create("Assets/Texture/タイトル画面UI2.png");

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
    if (m_background) m_background->Draw();
    
    (void)activeCamera;

    auto drawUIButton = [](Texture& tex, float centerX, float centerY, float targetWidth, float targetHeight)
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
    drawUIButton(m_cursor == 1 ? m_texPowerUpGreen : m_texPowerUpRed, centerX, startY + spacing * 1, buttonWidth, buttonHeight);
    drawUIButton(m_cursor == 2 ? m_texExitGreen : m_texExitRed, centerX, startY + spacing * 2, buttonWidth, buttonHeight);
    
}
