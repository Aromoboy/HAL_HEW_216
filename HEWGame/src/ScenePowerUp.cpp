#include "ScenePowerUp.h"

#include "TextureCache.h"
#include "PowerUpManager.h"
#include "DirectX.h"
#include "PerspectiveCamera.h"
#include "Core/Input.h"
#include "DebugConsole.h"
#include "PowerUpTextures.h"
#include <fstream>
#include <filesystem>
#include <string>

constexpr const char* kPowerUpConfigPath = "Assets/Config/PowerUp.cfg";
constexpr const char* kName = "PowerUpName=";
constexpr const char* kInfomation = "Infomation=";
constexpr const char* kTextureKey = "TextureName=";
constexpr const char* kMaxLevel = "MaxLevel=";
constexpr const char* kDefaultCost = "DefaultCost=";
constexpr const char* kCostAdditional = "CostAdditional=";
constexpr const char* kCostMagnification = "CostMagnification=";

enum PowerUpStatus
{
	Name,
	MaxLevel,
	DefaultCost,
	CostAdditional,
	CostMagnification,
	MaxStatusNum
};


namespace SceneSpace
{
	ScenePowerUp::ScenePowerUp() :
		m_cost(9999)
	{


                TextureCache& cache = TextureCache::Instance();
                const auto starKey = PowerUpTextures::BuildKey(PowerUpTextures::kStarPath);
                const auto costKey = PowerUpTextures::BuildKey(PowerUpTextures::kCostPath);
                const auto speedUpKey = PowerUpTextures::BuildKey(PowerUpTextures::kSpeedUpPath);
                const auto cakeSizeKey = PowerUpTextures::BuildKey(PowerUpTextures::kCakeSizePath);
                const auto whipKey = PowerUpTextures::BuildKey(PowerUpTextures::kWhipPath);
                const auto frameKey = PowerUpTextures::BuildKey(PowerUpTextures::kFramePath);
                const auto backFrameKey = PowerUpTextures::BuildKey(PowerUpTextures::kBackFramePath);
                cache.Load(starKey, GetDevice(), std::filesystem::path(PowerUpTextures::kStarPath).wstring());
                cache.Load(costKey, GetDevice(), std::filesystem::path(PowerUpTextures::kCostPath).wstring());
                cache.Load(speedUpKey, GetDevice(), std::filesystem::path(PowerUpTextures::kSpeedUpPath).wstring());
                cache.Load(cakeSizeKey, GetDevice(), std::filesystem::path(PowerUpTextures::kCakeSizePath).wstring());
                cache.Load(whipKey, GetDevice(), std::filesystem::path(PowerUpTextures::kWhipPath).wstring());
                cache.Load(frameKey, GetDevice(), std::filesystem::path(PowerUpTextures::kFramePath).wstring());
                cache.Load(backFrameKey, GetDevice(), std::filesystem::path(PowerUpTextures::kBackFramePath).wstring());

                m_camera = std::make_shared<CameraSpace::PerspectiveCamera>(45.0f);
                m_rig = std::make_unique<CameraRig>(m_camera);
                m_powerUpManager = std::make_unique<PowerUpManager>(GetUpdateScheduler(), cache, PowerUpManager::NodeFormat(3, 2));
                m_powerUpRenderer = std::make_unique<PowerUpRenderer>(cache);
                m_cursor = std::make_unique<PowerUpCursor>(cache.GetTexture(starKey), m_powerUpManager.get(), PowerUpManager::NodeFormat(3, 2), m_cost);
		m_target = std::make_shared<PowerUpTarget>(float3{ 0.0f, 0.0f, 0.0f });

		
		m_rig->Attach(m_target);
		
	
		auto registeredCamera = AddCamera(m_camera);
		
		
		SetActiveCamera(registeredCamera);


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

	ScenePowerUp::~ScenePowerUp()
	{
		m_powerUpRenderer->Shutdown();
		Unregister(*m_rig);
		Unregister(*m_cursor);
		Unregister(*m_powerUpManager);
	}

	void  ScenePowerUp::ConfigureUpdates()
	{
		if (auto* input = GetInput())
		{
			Register(*input);
		}
		Register(*m_powerUpManager);
		Register(*m_cursor);
		Register(*m_rig);
		
	}

	void ScenePowerUp::Update(Core::UpdateContext& ctx)
	{
		(void)ctx;

		m_target->MoveTarget(m_cursor->GetDrawPos(), m_powerUpManager->GetNodeDistance());
		
	}

	void ScenePowerUp::Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera)
	{

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

			// 説明文
			std::string infomation;
			std::getline(file, loadString);
			if (!GetValue(infomation, loadString, kInfomation)) continue;

			// テクスチャ名
			std::string textureKey;
			std::getline(file, loadString);
			if (!GetValue(textureKey, loadString, kTextureKey)) continue;
			
			// 最大レベル
			std::string maxLevel;
			std::getline(file, loadString);
			if (!GetValue(maxLevel, loadString, kMaxLevel)) continue;
			int finalMaxLevel = std::stoi(maxLevel);

			// 初期コスト
			std::string defaultCost;
			std::getline(file, loadString);
			if (!GetValue(defaultCost, loadString, kDefaultCost)) continue;
			int finalDefaultCost = std::stoi(defaultCost);

			// コスト加算数
			std::string costAdditional;
			std::getline(file, loadString);
			if (!GetValue(costAdditional, loadString, kCostAdditional)) continue;
			int finalCostAdditional = std::stoi(costAdditional);

			// コスト倍率
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

		// 一番最初に見つからなければ返す
		if (a_status.rfind(checkKey,0) != 0) return false;


		a_outValue = a_status.substr(keyWithEq.size());

		return true;
	}


	
	
}
