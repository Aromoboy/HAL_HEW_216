#include "TitleScene.h"

#include "Core/Input.h"
#include "Core/StepTimer.h"

#include "PerspectiveCamera.h"
#include "SceneGame.h"
#include "ScenePowerUp.h"
#include "SettingScene.h"
#include "EffectTestScene.h"
#include "DirectX.h"
#include "SceneManager.h"
#include "TitleRenderer.h"
#include "SceneResult.h"
 
#include <memory>

static int s_TitleBGMHandle = -1;

TitleScene::TitleScene(SceneSpace::SceneManager& manager)
    : m_manager(&manager)
    , m_camera(nullptr)
{
    
}

bool TitleScene::Load(Core::Input& input)
{
    (void)input;
    TitleCursor::NodeFormat format(2,2,3);

    TextureCache& cache = TextureCache::Instance();

    cache.Load("TitleCursor", GetDevice(),L"Assets/Texture/UI_whiteblock.png");
    cache.Load("GameSceneNode", GetDevice(), L"Assets/Texture/STARTUI.png");
    cache.Load("PowerUpSceneNode", GetDevice(), L"Assets/Texture/GREADUPUI.png");
    cache.Load("GameEndNode", GetDevice(), L"Assets/Texture/ENDUI.png");
    cache.Load("SelectGameSceneNode", GetDevice(), L"Assets/Texture/STARTUIselect.png");
    cache.Load("SelectPowerUpSceneNode", GetDevice(), L"Assets/Texture/GREADUPUIselect.png");
    cache.Load("SelectGameEndNode", GetDevice(), L"Assets/Texture/ENDUIselect.png");

    m_renderer = std::make_unique<TitleRenderer>();
    m_titleManager = std::make_unique<TitleManager>(&GetUpdateScheduler(), cache, format, input);

    return true;
}

void TitleScene::Update(Core::StepTimer& timer, Core::Input& input)
{
    if (m_video)
    {
        m_video->Update(timer.DeltaTime());
    }

    if (input.IsTrigger(Core::ActionId::Confirm))
    {
        if (m_manager)
        {
            m_manager->RequestSceneChange(
                std::make_unique<SceneGame>(*m_manager));
        }
        return;
    }

    RootUpdate(timer, input);
}

void TitleScene::Draw()
{
    // RootDraw triggers Draw(const Camera&) where the background is rendered.
    RootDraw();
}

void TitleScene::OnEnter()
{
    if (!m_camera)
    {
        m_camera = std::make_shared<CameraSpace::PerspectiveCamera>(45.0f);
    }

    auto registeredCamera = AddCamera(m_camera);
    if (!registeredCamera)
    {
        return;
    }

    SetActiveCamera(registeredCamera);

    if (!m_video)
    {
        VideoTexturePlayer::ShaderPaths shaderPaths;
        shaderPaths.vertexShader = "Assets/shaders/video_vs.cso";
        shaderPaths.pixelShader = "Assets/shaders/video_ps.cso";
        m_video = std::make_unique<VideoTexturePlayer>();
        if (!m_video->Initialize(GetDevice(), GetContext(), shaderPaths, "Assets/Video/Title.mp4"))
        {
            m_video.reset();
        }
    }

    /* UI ボタン用テクスチャ読み込み
    m_texStartRed.Create("Assets/Texture/タイトル画面UI5.png");
    m_texStartGreen.Create("Assets/Texture/タイトル画面UI6.png");
    m_texPowerUpRed.Create("Assets/Texture/タイトル画面UI3.png");
    m_texPowerUpGreen.Create("Assets/Texture/タイトル画面UI4.png");
    m_texExitRed.Create("Assets/Texture/タイトル画面UI1.png");
    m_texExitGreen.Create("Assets/Texture/タイトル画面UI2.png");
    */
    DirectX::XMMATRIX p = DirectX::XMMatrixOrthographicOffCenterLH(
        0.0f, 1280.0f, 720.0f, 0.0f, 0.0f, 1.0f);
    DirectX::XMFLOAT4X4 proj;
    DirectX::XMStoreFloat4x4(
        &proj,
        DirectX::XMMatrixOrthographicOffCenterLH(0.0f, 1280.0f, 720.0f, 0.0f, 0.0f, 1.0f)
    );
    Sprite::SetProjection(proj);

    m_shouldStopBGM = true;

    if (s_TitleBGMHandle == -1)
    {

        if (auto* soundMgr = SoundManagerEX::GetInstance())
        {

            s_TitleBGMHandle = soundMgr->PlaySoundEX(
                "Assets/Audio/BGM_Menu.wav",
                true, // Loop
                0.5f
            );

        }
    }

    m_rig = std::make_unique<CameraRig>(m_camera);
    m_target = std::make_shared<PowerUpTarget>(float3{ 0.0f, 0.0f, 0.0f });
    m_rig->Attach(m_target);
}

void TitleScene::OnExit()
{

    if (m_shouldStopBGM)
    {
        if (s_TitleBGMHandle != -1)
        {
            if (auto* soundMgr = SoundManagerEX::GetInstance())
            {
                soundMgr->StopSound(s_TitleBGMHandle);
            }
            s_TitleBGMHandle = -1;
        }
    }

    if (m_video)
    {
        m_video->Shutdown();
        m_video.reset();
    }
    if (m_inputRegistered)
    {
        if (auto* input = GetInput())
        {
            Unregister(*input);
        }
        m_inputRegistered = false;
    }

    if (m_titleManager)
    {
        Unregister(*m_titleManager);
        m_titleManager->UnregisterCursor();
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

    if (m_rig)
    {
        Register(*m_rig);
    }

    if (m_titleManager)
    {
        Register(*m_titleManager);
        m_titleManager->RegisterCursor();
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

    // Enter でゲーム開始
    if (input->IsTrigger(Core::ActionId::Confirm))
    {
        m_shouldStopBGM = true;
        if (m_manager)
        {
            m_manager->RequestSceneChange(
                std::make_unique<SceneGame>(*m_manager));
        }
        return;
    }

    if (input->IsTrigger('P'))
    {
        m_shouldStopBGM = false;
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<SceneSpace::ScenePowerUp>(*m_manager));
        }
        return;
    }

    if (input->IsTrigger(VK_F1))
    {
        m_shouldStopBGM = true;
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<EffectTestScene>(*m_manager));
        }
        return;
    }

    if (input->IsTrigger(VK_SPACE))
    {
        m_shouldStopBGM = false;
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<SceneSpace::SettingScene>(*m_manager));
        }
        return;
    }

    if (input->IsTrigger(Core::ActionId::Cancel))
    {
        m_shouldStopBGM = false;
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<TitleScene>(*m_manager));
        }
    }


    if (input->IsTrigger('O'))
    {
        m_shouldStopBGM = false;
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<SceneSpace::SceneResult>(*m_manager));
        }
    }

}

void TitleScene::Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
{
 
    (void)activeCamera;
    if (m_video)
    {
        m_video->Draw();
    }
    if (m_titleManager)
    {
        m_titleManager->DrawAll(*m_renderer);
    }


 

    
}
