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
    float dt = static_cast<float>(timer.DeltaTime());
    m_deltaTime = dt; // Draw�p�ɕێ�

    switch (m_titleState)
    {
    case TitleState::Idle:
        if (input.IsTrigger(Core::ActionId::Confirm)) {
            m_titleState = TitleState::CountDown;
            m_count = 3;
            m_countTimer = 0.0f;
            m_fadeAlpha = 0.0f; // �Ó]�J�n
        }
        break;

    case TitleState::CountDown:
        m_countTimer += dt;
        m_fadeAlpha += dt; // �Ó]����X�ɐi�߂�
        if (m_fadeAlpha > 1.0f) m_fadeAlpha = 1.0f;

        if (m_countTimer >= 1.0f) {
            m_countTimer = 0.0f;
            m_count--;
            if (m_count <= 0) {
                m_titleState = TitleState::StartEffect;
                m_startTimer = 0.0f;
            }
        }
        break;

    case TitleState::StartEffect:
        m_startTimer += dt;
        if (m_startTimer >= 1.0f) {
            m_manager->RequestSceneChange(std::make_unique<SceneGame>(*m_manager));
        }
        break;
    }
    RootUpdate(timer, input);
}

void TitleScene::Draw()
{
    Sprite::ScopedPassDefaults guard;
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

    // UI �{�^���p�e�N�X�`���ǂݍ���
    m_texStartRed.Create("Assets/Texture/�^�C�g�����UI5.png");
    m_texStartGreen.Create("Assets/Texture/�^�C�g�����UI6.png");
    m_texPowerUpRed.Create("Assets/Texture/�^�C�g�����UI3.png");
    m_texPowerUpGreen.Create("Assets/Texture/�^�C�g�����UI4.png");
    m_texExitRed.Create("Assets/Texture/�^�C�g�����UI1.png");
    m_texExitGreen.Create("Assets/Texture/�^�C�g�����UI2.png");
    m_texStartEffect.Create("Assets/Texture/start.png");
    m_texNumber[1].Create("Assets/Texture/3-1.png");
    m_texNumber[2].Create("Assets/Texture/2-1.png");
    m_texNumber[3].Create("Assets/Texture/1-1.png");


    DirectX::XMFLOAT4X4 proj;
    DirectX::XMStoreFloat4x4(
        &proj,
        DirectX::XMMatrixOrthographicOffCenterLH(0.0f, 1280.0f, 720.0f, 0.0f, 0.0f, 1.0f)
    );
    Sprite::SetProjection(proj);
   
    m_count = 3;
    m_countTimer = 0.0f;
    m_startTimer = 0.0f;

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


    if (m_titleState != TitleState::Idle) return;
    // �X�V�����i�L�[���͂ƃJ�[�\������j
    // ���㉺�ŃJ�[�\���ړ��A����ŃV�[���ؑ�

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
            m_manager->RequestSceneChange(
                std::make_unique<SceneGame>(*m_manager));
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
    if (m_background) m_background->Draw(0.0f, 1.0f);
    
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
    
    // �Ó]�i���j
    if (m_titleState != TitleState::Idle)
    {
        Sprite::SetOffset({ 640,360 });
        Sprite::SetSize({ 1280,720 });
        Sprite::SetColor({ 0,0,0,m_fadeAlpha });
        Sprite::Draw();
    }

    // �J�E���g�_�E��
    if (m_titleState == TitleState::CountDown && m_count > 0)
    {
        Sprite::SetTexture(&m_texNumber[m_count]);
        Sprite::SetOffset({ 640,360 });
        Sprite::SetSize({ 256,256 });
        Sprite::SetColor({ 1,1,1,1 });
        Sprite::Draw();
    }

    // START
    if (m_titleState == TitleState::StartEffect)
    {
        Sprite::SetTexture(&m_texStartEffect);
        Sprite::SetOffset({ 640,360 });
        Sprite::SetSize({ 600,200 });
        Sprite::SetColor({ 1,1,1,1 });
        Sprite::Draw();
    }
}
