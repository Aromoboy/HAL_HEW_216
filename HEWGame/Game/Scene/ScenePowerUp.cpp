#include "ScenePowerUp.h"

#include "DirectX.h"
#include "PerspectiveCamera.h"
#include "PowerUpManager.h"
#include "TitleScene.h"
#include "TextureCache.h"
#include "AppGame.h"

#include "Core/Input.h"
#include "Core/StepTimer.h"

#include "ConfigSectionReader.h"
#include "GameplayConfig.h"

#include <sstream>
#include <string>
#include <vector>

namespace SceneSpace
{

    float kBackMoveSpeed = 0.2f;
    ScenePowerUp::ScenePowerUp(SceneSpace::SceneManager& manager)
        : m_manager(&manager)
        , m_backUVProgress(0.0f)
        {
        }

        bool ScenePowerUp::Load(Core::Input& input)
        {
                (void)input;

                TextureCache& cache = App::Game::Instance().GetTextureCache();
                cache.Load("Star", GetDevice(), L"Assets/Texture/UI_Cakeblock_backeffect.png");
                cache.Load("Cost", GetDevice(), L"Assets/Texture/upgrade_UI_pointUP_icon.png");
                cache.Load("SpeedUp", GetDevice(), L"Assets/Texture/upgrade_UI_MoveSpeed_icon.png");
                cache.Load("CakeSize", GetDevice(), L"Assets/Texture/upgrade_UI_cakesizeUpai_icon.png");
                cache.Load("Whip", GetDevice(), L"Assets/Texture/upgrade_UI_cream_plus_icon.png");
                cache.Load("Frame", GetDevice(), L"Assets/Texture/UI_whiteblock.png");
                cache.Load("BackFrame", GetDevice(), L"Assets/Texture/UI_Cakeblock.png");
                cache.Load("Number", GetDevice(), L"Assets/Texture/number.png");         
                cache.Load("PowerUpBG", GetDevice(), L"Assets/Texture/upgrade_menu_background.png");
                cache.Load("Frame", GetDevice(), L"Assets/Texture/UI_whiteblock.png");
                cache.Load("SweetsSpawnSpeed", GetDevice(), L"Assets/Texture/upgrade_UI_fruit_Speed_icon.png");
                cache.Load("SweetsSpawnQuantity", GetDevice(), L"Assets/Texture/upgrade_UI_fruit_UP_icon.png");
                cache.Load("PointAmount", GetDevice(), L"Assets/Texture/upgrade_UI_pointUP_icon.png");

                std::unordered_map<std::string, EffectCreateData> effectDatas;
                effectDatas.try_emplace("PowerUp", EffectCreateData(u"Assets/Effect/sweets_catch_effect.efkefc", 0.03f));
                m_effectSystem = std::make_unique<EffectSystem>(m_manager->GetEffectRenderer(), m_manager->GetEffectManager(), effectDatas);
                m_camera = std::make_shared<CameraSpace::PerspectiveCamera>(45.0f);
                m_rig = std::make_unique<CameraRig>(m_camera);
                m_powerUpManager = std::make_unique<PowerUpManager>(GetUpdateScheduler(), cache, PowerUpManager::NodeFormat(3, 2));
                m_powerUpRenderer = std::make_unique<PowerUpRenderer>(cache);
                m_cursor = std::make_unique<PowerUpCursor>(cache.Get("Star"), m_powerUpManager.get(), PowerUpManager::NodeFormat(3, 2), m_cost, input, *m_effectSystem);
                m_target = std::make_shared<PowerUpTarget>(float3{ 0.0f, 0.0f, 0.0f });

                
                m_rig->Attach(m_target);

                LoadPowerUpFile();

                m_powerUpRenderer->Initialize(PowerUpHudLayoutConfig{ 512u,
                        240u,
                        32.0f,
                        12.0f,
                        28.0f,
                        5.0f,
                        1.5f,
                        2.0f,
                        DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
                        DirectX::XMFLOAT4(1.0f, 0.55f, 0.25f, 1.0f) });
                return true;
        }

        void ScenePowerUp::Update(Core::StepTimer& timer, Core::Input& input)
        {
                RootUpdate(timer, input);
                
        }

        void ScenePowerUp::Draw()
        {
            SetDepthTest(false);
                RootDraw();
        }

        void ScenePowerUp::OnEnter()
        {
                if (!m_camera)
                {
                        return;
                }

                auto registeredCamera = AddCamera(m_camera);

                if (registeredCamera)
                {
                        SetActiveCamera(registeredCamera);
                }
        }

        void ScenePowerUp::OnExit()
        {
                ShutdownPowerUpSystems();
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
        }

        void  ScenePowerUp::ConfigureUpdates()
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

                if (m_powerUpManager)
                {
                        Register(*m_powerUpManager);
                }
                if (m_cursor)
                {
                        Register(*m_cursor);
                }
                if (m_rig)
                {
                        Register(*m_rig);
                }
                if (m_effectSystem)
                {
                        Register(*m_effectSystem);
                }
        }

        void ScenePowerUp::Update(const Core::UpdateContext& ctx)
        {
                (void)ctx;
                Core::Input* input = GetInput();
                if (input && input->IsTrigger(Core::ActionId::Cancel))
                {
                        if (m_manager)
                        {
                                m_manager->RequestSceneChange(std::make_unique<TitleScene>(*m_manager));
                        }

                        return;
                }

                if (!m_target || !m_cursor || !m_powerUpManager)
                {
                        return;
                }

                m_target->MoveTarget(m_cursor->GetDrawPos(), m_powerUpManager->GetNodeDistance());
#if 0
                m_backUVProgress += ctx.dt * kBackMoveSpeed;
#endif // 0

                
        }

        void ScenePowerUp::Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera)
        {
                if (!m_powerUpRenderer || !m_powerUpManager || !m_cursor)
                {
                        return;
                }
                m_powerUpRenderer->DrawBackground(*activeCamera, m_backUVProgress);
                m_powerUpRenderer->DrawHud(*activeCamera);
                m_powerUpRenderer->DrawCost(m_cost, *activeCamera);
                m_cursor->Draw(*m_powerUpRenderer, *activeCamera);
                m_powerUpManager->DrawAll(*m_powerUpRenderer, *activeCamera);
                SetDepthTest(true);
                m_effectSystem->Draw(activeCamera.get());
        }

        bool ScenePowerUp::LoadPowerUpFile()
        {
                std::vector<Config::SectionLine> lines;
                if (!Config::ReadSectionLines(GameplayConfig::kConfigPath, GameplayConfig::kSectionPowerUps, lines))
                {
                        return false;
                }

                int loaded = 0;
                for (const auto& entry : lines)
                {
                        if (entry.key != "entry")
                        {
                                continue;
                        }

                        std::vector<std::string> tokens;
                        std::stringstream ss(entry.value);
                        std::string token;
                        while (std::getline(ss, token, ','))
                        {
                                token = Config::Trim(token);
                                if (!token.empty())
                                {
                                        tokens.push_back(token);
                                }
                        }

                        if (tokens.size() < 7)
                        {
                                continue;
                        }

                        try
                        {
                                const std::string& name = tokens[0];
                                const std::string& info = tokens[1];
                                const std::string& textureKey = tokens[2];
                                const int maxLevel = std::stoi(tokens[3]);
                                const int defaultCost = std::stoi(tokens[4]);
                                const int costAdditional = std::stoi(tokens[5]);
                                const float costMagnification = std::stof(tokens[6]);

                                m_powerUpManager->AddNode(name, info, textureKey, 0, maxLevel, defaultCost, costAdditional, costMagnification);
                                ++loaded;
                        }
                        catch (const std::exception&)
                        {
                                continue;
                        }
                }

                return loaded > 0;
        }

        void ScenePowerUp::ShutdownPowerUpSystems()
        {
                if (m_powerUpRenderer)
                {
                        m_powerUpRenderer->Shutdown();
                }

                if (m_rig)
                {
                        Unregister(*m_rig);
                }

                if (m_cursor)
                {
                        Unregister(*m_cursor);
                }

                if (m_powerUpManager)
                {
                        Unregister(*m_powerUpManager);
                }

                if (m_effectSystem)
                {
                        Unregister(*m_effectSystem);
                }
                m_target.reset();
                m_rig.reset();
                m_cursor.reset();
                m_powerUpManager.reset();
                m_powerUpRenderer.reset();
                m_camera.reset();
                m_effectSystem.reset();
        }
}
