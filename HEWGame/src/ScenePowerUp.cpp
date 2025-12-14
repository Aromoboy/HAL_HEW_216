#include "ScenePowerUp.h"

#include "DirectX.h"
#include "PerspectiveCamera.h"
#include "PowerUpManager.h"
#include "TitleScene.h"
#include "TextureCache.h"

#include "Core/Input.h"
#include "Core/StepTimer.h"

#include <filesystem>
#include <fstream>
#include <string>

constexpr const char* kPowerUpConfigPath = "Assets/Config/PowerUp.cfg";
constexpr const char* kName = "PowerUpName=";
constexpr const char* kInfomation = "Infomation=";
constexpr const char* kTextureKey = "TextureName=";
constexpr const char* kMaxLevel = "MaxLevel=";
constexpr const char* kDefaultCost = "DefaultCost=";
constexpr const char* kCostAdditional = "CostAdditional=";
constexpr const char* kCostMagnification = "CostMagnification=";

namespace SceneSpace
{
        ScenePowerUp::ScenePowerUp(SceneSpace::SceneManager& manager)
                : m_manager(&manager)
        {
        }

        void ScenePowerUp::Load(Core::Input& input)
        {
                (void)input;

                TextureCache& cache = TextureCache::Instance();
                cache.Load("Star", GetDevice(), L"Assets/Texture/Star.png");
                cache.Load("Cost", GetDevice(), L"Assets/Texture/upgrade_UI_pointUP_icon.png");
                cache.Load("SpeedUp", GetDevice(), L"Assets/Texture/upgrade_UI_MoveSpeed_icon.png");
                cache.Load("CakeSize", GetDevice(), L"Assets/Texture/upgrade_UI_cakesizeUpai_icon.png");
                cache.Load("Whip", GetDevice(), L"Assets/Texture/upgrade_UI_cream_plus_icon.png");
                cache.Load("Frame", GetDevice(), L"Assets/Texture/UI_whiteblock.png");
                cache.Load("BackFrame", GetDevice(), L"Assets/Texture/UI_Cakeblock.png");

                m_camera = std::make_shared<CameraSpace::PerspectiveCamera>(45.0f);
                m_rig = std::make_unique<CameraRig>(m_camera);
                m_powerUpManager = std::make_unique<PowerUpManager>(GetUpdateScheduler(), cache, PowerUpManager::NodeFormat(3, 2));
                m_powerUpRenderer = std::make_unique<PowerUpRenderer>(cache);
                m_cursor = std::make_unique<PowerUpCursor>(cache.Get("Star"), m_powerUpManager.get(), PowerUpManager::NodeFormat(3, 2), m_cost);
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
                        DirectX::XMFLOAT4(1.0f, 0.55f, 0.25f, 1.0f),
                        3.0f,
                        32.0f,
                        DirectX::XMFLOAT4(0.95f, 0.95f, 1.0f, 0.8f) });
        }

        void ScenePowerUp::Update(Core::StepTimer& timer, Core::Input& input)
        {
                RootUpdate(timer, input);
        }

        void ScenePowerUp::Draw()
        {
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
                UnregisterSceneUpdateNode();
                UnregisterCameraUpdateNode();
        }

        void  ScenePowerUp::ConfigureUpdates()
        {
                if (auto* input = GetInput())
                {
                        Register(*input);
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

        }

        void ScenePowerUp::Update(Core::UpdateContext& ctx)
        {
                if (ctx.input.IsTrigger(Core::ActionId::Cancel))
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

        }

        void ScenePowerUp::Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera)
        {
                if (!m_powerUpRenderer || !m_powerUpManager || !m_cursor)
                {
                        return;
                }

                m_powerUpRenderer->DrawHud(*activeCamera);
                m_powerUpManager->DrawAll(*m_powerUpRenderer, *activeCamera);
                m_cursor->Draw(*m_powerUpRenderer, *activeCamera);
        }

        bool ScenePowerUp::LoadPowerUpFile()
        {
                std::ifstream file(kPowerUpConfigPath);

                std::string loadString;

                if (!std::filesystem::exists(kPowerUpConfigPath))
                {
                        std::ofstream ofs(kPowerUpConfigPath);

                        ofs << "PowerUpName=Cream\n";
                        ofs << "MaxLevel=5\n";
                        ofs << "DefaultCost=100\n";
                        ofs << "CostAdditional=20\n";
                        ofs << "CostMagnification=1.0\n";
                }

                while(std::getline(file, loadString))
                {
                        std::string name;

                        if (!GetValue(name, loadString, kName)) continue;

                        std::string infomation;
                        std::getline(file, loadString);
                        if (!GetValue(infomation, loadString, kInfomation)) continue;

                        std::string textureKey;
                        std::getline(file, loadString);
                        if (!GetValue(textureKey, loadString, kTextureKey)) continue;

                        std::string maxLevel;
                        std::getline(file, loadString);
                        if (!GetValue(maxLevel, loadString, kMaxLevel)) continue;
                        int finalMaxLevel = std::stoi(maxLevel);

                        std::string defaultCost;
                        std::getline(file, loadString);
                        if (!GetValue(defaultCost, loadString, kDefaultCost)) continue;
                        int finalDefaultCost = std::stoi(defaultCost);

                        std::string costAdditional;
                        std::getline(file, loadString);
                        if (!GetValue(costAdditional, loadString, kCostAdditional)) continue;
                        int finalCostAdditional = std::stoi(costAdditional);

                        std::string costMagnification;
                        std::getline(file, loadString);
                        if (!GetValue(costMagnification, loadString, kCostMagnification)) continue;
                        float finalCostMagnification = std::stof(costMagnification);


                        m_powerUpManager->AddNode(name, infomation, textureKey, 0, finalMaxLevel, finalDefaultCost, finalCostAdditional, finalCostMagnification);
                }


                return true;
        }

        bool ScenePowerUp::GetValue(std::string& a_outValue, std::string a_status, const char* checkKey)
        {

                std::string keyWithEq = checkKey;

                if (a_status.rfind(checkKey,0) != 0) return false;


                a_outValue = a_status.substr(keyWithEq.size());

                return true;
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

                m_target.reset();
                m_rig.reset();
                m_cursor.reset();
                m_powerUpManager.reset();
                m_powerUpRenderer.reset();
                m_camera.reset();
        }
}

