#include "ScenePowerUp.h"

#include "TextureCache.h"
#include "PowerUpManager.h"
#include "DirectX.h"
#include "PerspectiveCamera.h"
#include "Core/Input.h"
#include "DebugConsole.h"
#include <fstream>
#include <sstream>
#include <string>

constexpr const char* kPowerUpConfigPath = "Assets/Config/PowerUp.cfg";
constexpr const char* kNameKey = "PowerUpName=";
constexpr const char* kMaxLevel = "MaxLLevel=";
constexpr const char* kDefaultCost = "DefaultCost=";
constexpr const char* kCostAdditional = "costAdditional=";
constexpr const char* kCostMagnification = "costMagnification=";

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


		LoadPowerUpFile();

		TextureCache& cache = TextureCache::Instance();
		cache.Load("Star", GetDevice(), L"Assets/Texture/Star.png");
		cache.Load("Cream", GetDevice(), L"Assets/Texture/upgrade_UI_cream_plus_icon.png");
		cache.Load("Frame", GetDevice(), L"Assets/Texture/UI_whiteblock.png");
		cache.Load("BackFrame", GetDevice(), L"Assets/Texture/UI_Cakeblock.png");


		// 各ファイルを読み込む
		m_camera = std::make_shared<CameraSpace::PerspectiveCamera>(45.0f);
		m_rig = std::make_unique<CameraRig>(m_camera);
		m_powerUpManager = std::make_unique<PowerUpManager>(GetUpdateScheduler(), cache, PowerUpManager::NodeFormat(3, 2));
		m_spriteRenderer = std::make_unique<SpriteRenderer>();
		m_cursor = std::make_unique<PowerUpCursor>(cache.Get("Star"), m_powerUpManager.get(), PowerUpManager::NodeFormat(3, 2), m_cost);
		m_target = std::make_shared<PowerUpTarget>(float3{ 0.0f, 0.0f, 0.0f });

		
		m_rig->Attach(m_target);
		
	
		auto registeredCamera = AddCamera(m_camera);
		
		
		SetActiveCamera(registeredCamera);

		m_powerUpManager->AddNode("Cream", 0, 10, 50, 10, 1.1);
		m_powerUpManager->AddNode("Cream", 0, 10, 50, 10, 1.1);
		m_powerUpManager->AddNode("Cream", 0, 10, 50, 10, 1.1);
		m_powerUpManager->AddNode("Cream", 0, 10, 50, 10, 1.1);
		m_powerUpManager->AddNode("Cream", 0, 10, 50, 10, 1.1);
		m_powerUpManager->AddNode("Cream", 0, 10, 50, 10, 1.1);
		m_powerUpManager->AddNode("Cream", 0, 10, 50, 10, 1.1);
		m_powerUpManager->AddNode("Cream", 0, 10, 50, 10, 1.1);
		m_powerUpManager->AddNode("Cream", 0, 10, 50, 10, 1.1);
		m_powerUpManager->AddNode("Cream", 0, 10, 50, 10, 1.1);
		m_powerUpManager->AddNode("Cream", 0, 10, 50, 10, 1.1);

		

	}

	ScenePowerUp::~ScenePowerUp()
	{
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
		TextureCache& cache = TextureCache::Instance();

		Transform transform;
		transform.position  = float3{ 0.0f,0.0f,100.0f };
		transform.rotation  = float3{ 0.0f,0.0f,0.0f };
		transform.scale		= float3{ 100.0f,100.0f,100.0f };


		m_powerUpManager->DrawAll(*m_spriteRenderer, *activeCamera);
		m_cursor->Draw(*m_spriteRenderer, *activeCamera);
	}

	bool ScenePowerUp::LoadPowerUpFile()
	{
		std::ifstream file(kPowerUpConfigPath);
		
		std::string nameKey = "PowerUp=";
		std::stringstream ss(nameKey);

		while(std::getline(file, nameKey))
		{
			

			std::string key[MaxStatusNum];
			std::string value[MaxStatusNum];

			std::string status;

			//m_powerUpManager->AddNode(value[Name], 0, std::stoi(value[MaxLevel]), std::stoi(value[DefaultCost]), std::stoi(value[DefaultCost]), std::stof(value[DefaultCost]));
		}

		return false;
	}

	bool ScenePowerUp::GetValue(std::string& a_outValue, std::string a_status, const char* checkKey)
	{


		// 一番最初に見つからなければ返す
		if (a_status.rfind(checkKey,0) != 0) return false;

		std::stringstream ss(a_status);

		// 一応保持
		std::string key;

		std::getline(ss, key, '=');
		std::getline(ss, a_outValue);

		return true;
	}


	
	
}