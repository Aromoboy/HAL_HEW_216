#include "ScenePowerUp.h"

#include "DirectX.h"
#include "PerspectiveCamera.h"
#include "PowerUpManager.h"
#include "PowerUpEffectConfig.h"
#include "TitleScene.h"
#include "TextureCache.h"
#include "AppGame.h"
#include "Timer.h"
#include "SoundManagerEX.h"

#include "Core/Input.h"
#include "Core/StepTimer.h"

#include "ConfigSectionReader.h"
#include "GameplayConfig.h"
#include "Debug.h"


#include <sstream>
#include <string>
#include <vector>

extern int s_TitleBGMHandle;

namespace
{
        PlayerData::Data CreateDefaultPlayerEntry()
        {
                PlayerData::Data entry{};
                entry.nPoints = 0;
                for (const auto& powerUpEntry : PowerUpEffectConfig::Entries())
                {
                        entry.levels[powerUpEntry.type] = 0;
                }
                return entry;
        }
}

namespace SceneSpace
{

    float kBackMoveSpeed = 0.2f;
    constexpr const char* kLogPrefix = "[ScenePowerUp] ";
    ScenePowerUp::ScenePowerUp(SceneSpace::SceneManager& manager)
        : m_manager(&manager)
        , m_backUVProgress(0.0f)
        , m_cost(0)
        , m_timer(5.0f)
        , m_starTimer(0.2f)
        {
        }

        bool ScenePowerUp::Load(Core::Input& input)
        {
                (void)input;

                const bool loaded = m_playerData.LoadData();
                m_playerEntries = m_playerData.GetEntries();
                if (!loaded || m_playerEntries.empty())
                {
                        m_playerEntries.clear();
                        m_playerEntries.push_back(CreateDefaultPlayerEntry());
                        m_playerData.SetEntries(m_playerEntries);
                        m_playerData.SaveData();
                }
                m_cost = m_playerEntries.front().nPoints;

                TextureCache& cache = App::Game::Instance().GetTextureCache();
                cache.Load("Star", GetDevice(), L"Assets/Texture/UI_Cakeblock_backeffect.png");
                cache.Load("Cost", GetDevice(), L"Assets/Texture/upgrade_UI_pointUP_icon.png");
                cache.Load("SpeedUp", GetDevice(), L"Assets/Texture/upgrade_UI_MoveSpeed_icon.png");
                cache.Load("Whip", GetDevice(), L"Assets/Texture/upgrade_UI_cream_plus_icon.png");
                cache.Load("Frame", GetDevice(), L"Assets/Texture/UI_whiteblock.png");
                cache.Load("BackFrame", GetDevice(), L"Assets/Texture/UI_Cakeblock.png");
                cache.Load("Number", GetDevice(), L"Assets/Texture/number.png");
                cache.Load("PowerUpBG", GetDevice(), L"Assets/Texture/upgrade_menu_background.png");
                cache.Load("Frame", GetDevice(), L"Assets/Texture/UI_whiteblock.png");
                cache.Load("SweetsSpawnSpeed", GetDevice(), L"Assets/Texture/upgrade_UI_fruit_Speed_icon.png");
                cache.Load("SweetsSpawnQuantity", GetDevice(), L"Assets/Texture/upgrade_UI_fruit_UP_icon.png");
                cache.Load("PointAmount", GetDevice(), L"Assets/Texture/upgrade_UI_pointUP_icon _remake.png");
                cache.Load("Timer", GetDevice(), L"Assets/Texture/upgrade_UI_time_bonus_icon.png");
                cache.Load("Fairy", GetDevice(), L"Assets/Texture/PowerUpBackFairy.png");

                std::unordered_map<std::string, EffectCreateData> effectDatas;
                effectDatas.try_emplace("PowerUp", EffectCreateData(u"Assets/Effect/sweets_catch_effect.efkefc", 0.03f));
                effectDatas.try_emplace("Bubble", EffectCreateData(u"Assets/Effect/PowerUpEffect/bubble_effect.efkefc", 0.05f));
                effectDatas.try_emplace("Fruit", EffectCreateData(u"Assets/Effect/PowerUpEffect/Banana_strawberry_animation.efkefc", 0.5f));
                effectDatas.try_emplace("Star", EffectCreateData(u"Assets/Effect/PowerUpEffect/star_background.efkefc", 0.175f));
                m_effectSystem = std::make_unique<EffectSystem>(m_manager->GetEffectRenderer(), m_manager->GetEffectManager(), effectDatas);
                m_camera = std::make_shared<CameraSpace::PerspectiveCamera>(45.0f);
                m_rig = std::make_unique<CameraRig>(m_camera);
                m_powerUpManager = std::make_unique<PowerUpManager>(GetUpdateScheduler(), cache, PowerUpManager::NodeFormat(3, 2));
                m_powerUpRenderer = std::make_unique<PowerUpRenderer>(cache);
                auto onPurchase = [this](int index)
                {
                        if (!m_powerUpManager || m_playerEntries.empty())
                        {
                                return;
                        }

                        const std::string key = m_powerUpManager->GetName(index);
                        const auto effectEntry = PowerUpEffectConfig::FindByKey(key);
                        if (!effectEntry)
                        {
                                return;
                        }

                        if (auto* soundMgr = SoundManagerEX::GetInstance())
                        {
                            soundMgr->PlaySoundEX("Assets/Audio/SE_Purchase.wav", false, 0.7f);
                        }

                        PlayerData::Data& entry = m_playerEntries.front();
                        entry.levels[effectEntry->type] = m_powerUpManager->GetLevel(index);
                        entry.nPoints = m_cost;
                        m_playerData.SetEntries(m_playerEntries);
                        m_playerData.SaveData();
                };
                m_cursor = std::make_unique<PowerUpCursor>(cache.Get("Star"), m_powerUpManager.get(), PowerUpManager::NodeFormat(3, 2), m_cost, input, *m_effectSystem, std::move(onPurchase));
                m_target = std::make_shared<PowerUpTarget>(float3{ 0.0f, 0.0f, 0.0f });

                EffectSystem::EffectCreateRequest FruitRequest(
                    "Fruit",
                    float3{ 0.55f, -0.3f, 1.0f },
                    float3{ 0.0f, 0.0f, 0.0f },
                    0);
                //m_effectSystem->SpawnEffect(FruitRequest);

                EffectSystem::EffectCreateRequest BubbleRequest(
                    "Bubble",
                    float3{ -0.6f, -0.2f, 1.0f },
                    float3{ 0.0f, 0.0f, 0.0f },
                    0);
                //m_effectSystem->SpawnEffect(BubbleRequest);

                EffectSystem::EffectCreateRequest StarRequest(
                    "Star",
                    float3{ 0.0f, 0.0f, 1.0f },
                    float3{ 0.0f, 0.0f, 0.0f },
                    0);
                //m_effectSystem->SpawnEffect(StarRequest);

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

                VideoBackgroundRenderer::ShaderPaths shaderPaths{
                    "Assets/shaders/video_vs.cso",
                    "Assets/shaders/video_ps.cso"
                };
                m_videoBackground = std::make_unique<VideoBackgroundRenderer>();
                if (!m_videoBackground->Initialize(GetDevice(), GetContext(), shaderPaths, "Assets/Video/PowerUp.mp4"))
                {
                    m_videoBackground.reset();
                }
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

                if (s_TitleBGMHandle == -1)
                {
                    if (auto* soundMgr = SoundManagerEX::GetInstance())
                    {
                        s_TitleBGMHandle = soundMgr->PlaySoundEX("Assets/Audio/BGM_Menu.wav", true, 0.5f);
                    }
                }
        }

        void ScenePowerUp::OnExit()
        {

            if (m_playerEntries.empty())
            {
                m_playerEntries.push_back(CreateDefaultPlayerEntry());
            }

            PlayerData::Data& entry = m_playerEntries.front();
            if (m_powerUpManager)
            {
                const int nodeCount = m_powerUpManager->GetNodeCount();
                for (int i = 0; i < nodeCount; ++i)
                {
                    const std::string key = m_powerUpManager->GetName(i);
                    const auto effectEntry = PowerUpEffectConfig::FindByKey(key);
                    if (!effectEntry)
                    {
                        continue;
                    }
                    entry.levels[effectEntry->type] = m_powerUpManager->GetLevel(i);
                }
            }
            entry.nPoints = m_cost;
            m_playerData.SetEntries(m_playerEntries);
            if (!m_playerData.SaveData())
            {
                DebugLog::DebugMessage(std::string(kLogPrefix) + "Failed to save player data on exit.\n");
            }

                if (m_videoBackground)
                {
                    m_videoBackground->Shutdown();
                }
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
                if (m_videoBackground)
                {
                    m_videoBackground->Update(ctx.dt);
                }
                Core::Input* input = GetInput();
                m_timer.StepTime(ctx.dt);
                m_starTimer.StepTime(ctx.dt);
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

                if (m_effectSystem != nullptr && m_timer.GetCount())
                {
                    EffectSystem::EffectCreateRequest BubbleRequest(
                        "Bubble",
                        float3{ -0.6f, -0.2f, 1.0f },
                        float3{ 0.0f, 0.0f, 0.0f },
                        0);
                    //m_effectSystem->SpawnEffect(BubbleRequest);



                }
                if (m_effectSystem != nullptr && m_starTimer.GetCount())
                {
                    float2 pos;
                    pos.x = std::fmodf((float)rand() / 100.0f, 1.0f) - 0.5f;
                    pos.y = std::fmodf((float)rand() / 100.0f, 0.5f) - 0.25f;

                    EffectSystem::EffectCreateRequest StarRequest(
                        "Star",
                        float3{ pos.x, pos.y, 1.0f },
                        float3{ 0.0f, 0.0f, 0.0f },
                        0);
                    m_effectSystem->SpawnEffect(StarRequest);
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
                if (m_videoBackground)
                {
                    m_videoBackground->Draw();
                }
                m_powerUpRenderer->DrawHud(*activeCamera);
                m_powerUpRenderer->DrawCost(m_cost, *activeCamera);
                SetDepthTest(true);
                m_effectSystem->Draw(activeCamera.get());
                SetDepthTest(false);
                m_cursor->Draw(*m_powerUpRenderer, *activeCamera);
                m_powerUpManager->DrawAll(*m_powerUpRenderer, *activeCamera);
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
                                std::ostringstream warn;
                                warn << kLogPrefix << "Line " << entry.lineNumber
                                     << ": entry must provide name, info, texture, max level, default cost, additional cost, and magnification.\n";
                                DebugLog::DebugMessage(warn.str());
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

                                int currentLevel = 0;
                                if (!m_playerEntries.empty())
                                {
                                        const auto effectEntry = PowerUpEffectConfig::FindByKey(textureKey);
                                        if (effectEntry)
                                        {
                                                const auto levelIter = m_playerEntries.front().levels.find(effectEntry->type);
                                                if (levelIter != m_playerEntries.front().levels.end())
                                                {
                                                        currentLevel = levelIter->second;
                                                }
                                        }
                                }
                                m_powerUpManager->AddNode(name, info, textureKey, currentLevel, maxLevel, defaultCost, costAdditional, costMagnification);
                                ++loaded;
                        }
                        catch (const std::exception& ex)
                        {
                                std::ostringstream warn;
                                warn << kLogPrefix << "Line " << entry.lineNumber
                                     << ": failed to parse power-up entry values (" << ex.what() << ").\n";
                                DebugLog::DebugMessage(warn.str());
                                continue;
                        }
                }

                if (loaded == 0)
                {
                        std::ostringstream warn;
                        warn << kLogPrefix << "[Error] No power-up entries were loaded from section '"
                             << GameplayConfig::kSectionPowerUps << "'.\n";
                        DebugLog::DebugMessage(warn.str());
                        return false;
                }

                return true;
        }

        void ScenePowerUp::ShutdownPowerUpSystems()
        {
                if (m_powerUpRenderer)
                {
                        m_powerUpRenderer->Shutdown();
                }
                if (m_videoBackground)
                {
                    m_videoBackground->Shutdown();
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
                m_videoBackground.reset();
                m_camera.reset();
                m_effectSystem.reset();
        }
}
