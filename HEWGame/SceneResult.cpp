
#include "SceneResult.h"

#include "DirectX.h"
#include "Core/Input.h"
#include "Core/StepTimer.h"
#include "TitleScene.h"
#include "SceneGame.h"
#include "ScenePowerUp.h"
#include "AppGame.h"


constexpr float2 kResultCostOffset  = {  0.0f, -1.2f };
constexpr float2 kResultTowerOffset = { -1.2f,  1.1f };


namespace SceneSpace
{
    SceneResult::SceneResult(SceneManager& manager, int a_towerSize, int a_getCost)
        : m_manager(&manager)
        , m_towerSize(a_towerSize)
        , m_getCost(a_getCost)
    {
    }

    bool SceneResult::Load(Core::Input& input)
    {
        (void)input;

        // カメラ
        m_camera = std::make_shared<CameraSpace::BaseCamera>();
        m_rig = std::make_unique<CameraRig>(m_camera);

        // リザルトレンダラ
        m_resultRenderer = std::make_unique<ResultRenderer>();

        ResultLayoutConfig config{};
        config.textureWidth = 512;
        config.textureHeight = 256;
        config.fontSizePixels = 22;
        config.marginPixels = 20;
        config.normalTextColor = { 1,1,1,1 };
        config.warningTextColor = { 1,0.3f,0.3f,1 };

        m_resultRenderer->Initialize(config);

        // UI テクスチャ
        TextureCache& cache = TextureCache::Instance();
        
        std::string keys[3] =
        {
            "Assets/Texture/basket_UI_remake2.png",
            "Assets/Texture/basket_UI_remake3.png",
            "Assets/Texture/basket_UI_remake4.png"
        };

        std::wstring paths[3] =
        {
            L"Assets/Texture/basket_UI_remake2.png",
            L"Assets/Texture/basket_UI_remake3.png",
            L"Assets/Texture/basket_UI_remake4.png"
        };


        
        for (int i = 0; i < 3; i++)
        {
            m_menuTextures[i] = cache.Load(keys[i], GetDevice(), paths[i]);
        }

        m_pointAmount = cache.Load("Number", GetDevice(), L"Assets/Texture/number.png");

        m_menuSelectNodes = cache.Load("ResultSelect", GetDevice(), L"Assets/Texture/basket_UI_backeffect_remake.png");
        
        m_backTexture = cache.Load("ResultBack", GetDevice(), L"Assets/Texture/background_UI_remake.png");

        m_meter = cache.Load("Meter", GetDevice(), L"Assets/Texture/m.png");
       
        return true;
    }

    void SceneResult::OnEnter()
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

        if (auto soundMgr = SoundManagerEX::GetInstance())
        {
            soundMgr->PlaySoundEX("Assets/Audio/SE_ResultPop.wav", false, 0.5f);
        }

        m_videoPlayer = std::make_unique<VideoTexturePlayer>();
        VideoTexturePlayer::ShaderPaths shaderPaths{};
        shaderPaths.vertexShader = "Assets/shaders/video_vs.cso";
        shaderPaths.pixelShader = "Assets/shaders/video_ps.cso";
        if (!m_videoPlayer->Initialize(GetDevice(), GetContext(), shaderPaths, "Assets/Video/Result.mp4"))
        {
            m_videoPlayer.reset();
        }
        else
        {
            m_videoPlayer->SetInvertY(false);
            m_videoPlayer->SetScroll01(0.0f);
        }
        m_texStartRed.Create("Assets/Texture/タイトル画面UI5.png");
        m_texStartGreen.Create("Assets/Texture/タイトル画面UI6.png");
        m_texPowerUpRed.Create("Assets/Texture/タイトル画面UI3.png");
        m_texPowerUpGreen.Create("Assets/Texture/タイトル画面UI4.png");
        m_texExitRed.Create("Assets/Texture/タイトル画面.png");
        m_texExitGreen.Create("Assets/Texture/タイトル画面UI2.png");
        m_texStartEffect.Create("Assets/Texture/start.png");
        m_texNumber[1].Create("Assets/Texture/3-1.png");
        m_texNumber[2].Create("Assets/Texture/2-1.png");
        m_texNumber[3].Create("Assets/Texture/1-1.png");


        DirectX::XMFLOAT4X4 proj;
        DirectX::XMStoreFloat4x4(
            &proj,
            DirectX::XMMatrixOrthographicOffCenterLH(
                0.0f, 1280.0f,   // X: 左 → 右
                960.0f, 0.0f,   // Y: 上 → 下
                0.0f, 1.0f)
        );

        Sprite::SetProjection(proj);
        
    }

    void SceneResult::OnExit()
    {
        if (m_inputRegistered)
        {
            if (auto* input = GetInput())
            {
                Unregister(*input);
            }
            m_inputRegistered = false;
        }

        UnregisterSceneUpdateNode();
        UnregisterCameraUpdateNode();

        if (m_videoPlayer)
        {
            m_videoPlayer->Shutdown();
        }

        //m_cursor.reset();
        m_menuTextures = {};
        m_resultRenderer.reset();
        m_videoPlayer.reset();
        m_rig.reset();
        m_camera.reset();
    }

    void SceneResult::ConfigureUpdates()
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

        if (m_rig)
        {
            Register(*m_rig);
        }
    }

    void SceneResult::Update(Core::StepTimer& timer, Core::Input& input)
    {
        RootUpdate(timer, input);
    }

    void SceneResult::Update(const Core::UpdateContext& ctx)
    {
        if (m_videoPlayer)
        {
            m_videoPlayer->Update(ctx.dt);
        }

        Core::Input* input = GetInput();
        if (!input) return;

        float vertical = input->GetAxis(Core::ActionId::RotateYaw);

        static bool inputLock = false;

        if (!inputLock)
        {
            if (vertical > 0.5f)
            {
                m_cursorIndex--;
                if (m_cursorIndex < 0) m_cursorIndex = kMenuCount - 1;

                inputLock = true;

                if (auto* soundMgr = SoundManagerEX::GetInstance())
                {
                    soundMgr->PlaySoundEX("Assets/Audio/SE_Enter.wav", false, 0.5f);
                }
            }
            else if (vertical < -0.5f)
            {
                m_cursorIndex++;
                if (m_cursorIndex >= kMenuCount) m_cursorIndex = 0;

                inputLock = true;

                if (auto* soundMgr = SoundManagerEX::GetInstance())
                {
                    soundMgr->PlaySoundEX("Assets/Audio/SE_Enter.wav", false, 0.5f);
                }
            }

        }
        if (std::abs(vertical) < 0.2f)
        {
            inputLock = false;
        }

        if (input->IsTrigger(Core::ActionId::Confirm))
        {
            switch (m_cursorIndex)
            {
            case 0: // Retry
                m_manager->RequestSceneChange(
                    std::make_unique<SceneGame>(*m_manager));
                break;

            case 1: // PowerUp
                m_manager->RequestSceneChange(
                    std::make_unique<ScenePowerUp>(*m_manager));
                break;

            case 2: // Title
                m_manager->RequestSceneChange(
                    std::make_unique<TitleScene>(*m_manager));
                break;
            }
        }
    }

    void SceneResult::Draw()
    {
        SetDepthTest(false);
       
        Sprite::ScopedPassDefaults spritePass;

        {
            if (m_videoPlayer)
            {
                const auto& dxContext = GetDXContext();
                m_videoPlayer->SetViewportSize(static_cast<float>(dxContext.GetWidth()), static_cast<float>(dxContext.GetHeight()));
                m_videoPlayer->Draw();
            }

            RootDraw();   // ← Draw(const Camera&) が呼ばれる
        }
        SetDepthTest(true);
    }

    void SceneResult::Draw(const std::shared_ptr<CameraSpace::ICamera>&)
    {
        // リザルト情報
        ResultDisplayData data{};
        data.timeUp = true;
        data.remainingSeconds = 0.0f;
        data.score = 12345;
        data.sweetsCount = 12;


        const float baseX = 2.9f;   // 画面右側
        const float baseY = 1.7f;
        const float spacingx = -0.1f;
        const float spacingy = -1.95f;

        m_resultRenderer->Draw(m_backTexture, float2(-0.7f, 1.0f), float2(12.0f, 9.0f));

        for (int i = 0; i < 3; i++)
        {
            float x = baseX + spacingx * i;
            float y = baseY + spacingy * i;
            float3 color(1.0f, 1.0f, 1.0f);
            if (i == m_cursorIndex)
            {
                m_resultRenderer->Draw(m_menuSelectNodes, float2(baseX, y), float2(4.5f, 4.0f), float3(1.0f, 1.0f, 1.0f));
            }

            m_resultRenderer->Draw(m_menuTextures[i], float2(x, y), float2(5.0f, 4.0f), color);
        }
        
       
        int costCount = 0;
        int cost = m_getCost;
        if (cost)
        {
            while (cost)
            {
                float2 pos(kResultCostOffset.x - 0.7f * costCount, kResultCostOffset.y);
                float2 scale(1.0f, 1.0f);

                int drawNum = cost % 10;
                m_resultRenderer->Draw(m_pointAmount, pos, scale, float3(1.0F, 1.0F, 1.0F), 1.0F, float2((float)drawNum / 10.0f, 0.0f), float2(0.1f, 1.0f));
                cost /= 10;
                costCount++;
            }
        }
        else
        {
            float2 pos(kResultCostOffset.x, kResultCostOffset.y);
            float2 scale(1.0f, 1.0f);

            m_resultRenderer->Draw(m_pointAmount, pos, scale, float3(1.0F, 1.0F, 1.0F), 1.0F, float2(0.0f, 0.0f), float2(0.1f, 1.0f));
        }

        int towerCount = 0;
        int towerSize = m_towerSize;
        if (towerSize)
        {
            while (towerSize)
            {
                float2 pos(kResultTowerOffset.x - 1.0f * towerCount, kResultTowerOffset.y);
                float2 scale(1.4f, 1.4f);

                int drawNum = towerSize % 10;
                m_resultRenderer->Draw(m_pointAmount, pos, scale, float3(1.0F, 1.0F, 1.0F), 1.0F, float2((float)drawNum / 10.0f, 0.0f), float2(0.1f, 1.0f));
                towerSize /= 10;
                towerCount++;
            }
        }
        else
        {
            float2 pos(kResultTowerOffset.x, kResultTowerOffset.y);
            float2 scale(1.4f, 1.4f);

            m_resultRenderer->Draw(m_pointAmount, pos, scale, float3(1.0F, 1.0F, 1.0F), 1.0F, float2(0.0f, 0.0f), float2(0.1f, 1.0f));
        }

        float2 meterPos(kResultTowerOffset.x + 1.2f, kResultTowerOffset.y);
        float2 meterScale(1.4f, 1.4f);
        m_resultRenderer->Draw(m_meter, meterPos, meterScale, float3(1.0F, 1.0F, 1.0F), 1.0F);
        
    }

    
}
    
    
