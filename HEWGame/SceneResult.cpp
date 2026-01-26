
#include "SceneResult.h"

#include "DirectX.h"
#include "Core/Input.h"
#include "Core/StepTimer.h"
#include "TitleScene.h"
#include "SceneGame.h"
#include "ScenePowerUp.h"
#include "AppGame.h"

namespace SceneSpace
{
    SceneResult::SceneResult(SceneManager& manager)
        : m_manager(&manager)
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
        const char* paths[3] =
        {
            "Assets/Texture/basket_UI_remake2.png",
            "Assets/Texture/basket_UI_remake3.png",
            "Assets/Texture/basket_UI_remake4.png"
        };

        for (int i = 0; i < 3; i++)
        {
            m_menuTextures[i] = std::make_unique<Texture>();
            m_menuTextures[i]->Create(paths[i], true);
           

        }
        // カーソル（PowerUpCursor流用）
       /* m_cursor = std::make_unique<PowerUpCursor>();*/


       
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





        BackgroundRendererConfig cfg{};
        BackgroundRendererConfig::BackgroundLayer layer;
        layer.texturePath = "Assets/Texture/img_1769391385.png";  // 背景画像
        layer.parallaxFactor = 0.0f;
        layer.samplingMode = BackgroundRendererConfig::SamplingMode::Clamp;
        layer.tintColor = DirectX::XMFLOAT4(1, 1, 1, 1);
        cfg.layers.push_back(layer);

        if (!cfg.layers.empty())
        {
            m_background = std::make_unique<BackgroundRenderer>();
            if (!m_background->Initialize(cfg))
            {
                m_background.reset();
            }
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

        //m_cursor.reset();
        m_menuTextures = {};
        m_resultRenderer.reset();
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

    void SceneResult::Update(const Core::UpdateContext&)
    {
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
            }
            else if (vertical < -0.5f)
            {
                m_cursorIndex++;
                if (m_cursorIndex >= kMenuCount) m_cursorIndex = 0;


                inputLock = true;
            }
            if (std::abs(vertical) < 0.2f)
            {
                inputLock = false;
            }
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

        DirectX::XMFLOAT4X4 proj;
        DirectX::XMStoreFloat4x4(
            &proj,
            DirectX::XMMatrixOrthographicOffCenterLH(
                0.0f, 1280.0f,
                960.0f, 0.0f,
                0.0f, 1.0f)
        );
        Sprite::SetProjection(proj);
        Sprite::ScopedPassDefaults spritePass;

        
            if (m_background)
            {
                m_background->Draw(0.0f, 1.0f);
            }

         
            RootDraw();   // ← Draw(const Camera&) が呼ばれる
    

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

        m_resultRenderer->Draw(data);

       // const float baseX = 0.65f;
       // const float baseY = -0.2f;
       ////const float intervalY = 0.25f;
       //
       // const float baseY = 300.0f;
       // const float spacing = 110.0f;
        const float baseX = 900.0f;   // 画面右側
        const float baseY = 250.0f;
        const float spacing = 120.0f;
        for (int i = 0; i < 3; i++)
        {
            float y = baseY + spacing * i;
            if (i == m_cursorIndex)
            {
                y -= 10.0f; // 選択中だけ少し動かす
            }

            Sprite::SetTexture(m_menuTextures[i].get());
            Sprite::SetSize({ 400.0f,120.0f });
            Sprite::SetOffset({ 640.0f, y });

            if (i == m_cursorIndex)
            {
                Sprite::SetColor({ 0.3f, 1.0f, 0.3f, 1.0f });
            }
            else
            {
                Sprite::SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            }

            Sprite::Draw();
        }

        
        
    }

    
}
    
    

