#pragma once

#include <memory>

#include "Scene.h"
#include "SpriteRenderer.h"
#include "BaseCamera.h"
#include "PowerUpCursor.h"
#include "CameraRig.h"
#include "PowerUpTarget.h"
#include "PowerUpRenderer.h"

class PowerUpManager;

namespace SceneSpace
{
	class ScenePowerUp : public SceneSpace::Scene
	{
	public:
		ScenePowerUp();
		~ScenePowerUp();

		void ConfigureUpdates() override;
		void Update(Core::UpdateContext& ctx) final;
		void Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera) final;

	private:
		std::unique_ptr<PowerUpManager> m_powerUpManager;
		std::unique_ptr<PowerUpRenderer> m_powerUpRenderer;
		std::shared_ptr<CameraSpace::BaseCamera> m_camera;
		std::unique_ptr<PowerUpCursor> m_cursor;
		std::unique_ptr<CameraRig> m_rig;
		std::shared_ptr<PowerUpTarget> m_target;

		bool LoadPowerUpFile();

		bool GetValue(std::string& a_outValue, std::string status, const char*checkKey);

		int m_cost;


	};
}