#pragma once

#include <memory>
#include <unordered_map>

#include "BaseCamera.h"
#include "CameraRig.h"
#include "PowerUpCursor.h"
#include "PowerUpRenderer.h"
#include "PowerUpTarget.h"
#include "Scene.h"
#include "SceneManager.h"
#include "SpriteRenderer.h"
#include "EffectSystem.h"
#include "Timer.h"



class PowerUpManager;

namespace SceneSpace
{
        class ScenePowerUp : public SceneSpace::Scene, public SceneSpace::IScene
        {
        public:
                explicit ScenePowerUp(SceneSpace::SceneManager& manager);
                ~ScenePowerUp() = default;

                bool Load(Core::Input& input) override;
                void Update(Core::StepTimer& timer, Core::Input& input) override;
                void Draw() override;
                void OnEnter() override;
                void OnExit() override;

                void ConfigureUpdates() override;
                void Update(const Core::UpdateContext& ctx) final;
                void Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera) final;

        private:
                bool LoadPowerUpFile();
                void ShutdownPowerUpSystems();

                float m_backUVProgress;
                SceneManager* m_manager = nullptr;
                std::unique_ptr<PowerUpManager> m_powerUpManager;
                std::unique_ptr<PowerUpRenderer> m_powerUpRenderer;
                std::shared_ptr<CameraSpace::BaseCamera> m_camera;
                std::unique_ptr<EffectSystem> m_effectSystem;
                std::unique_ptr<PowerUpCursor> m_cursor;
                std::unique_ptr<CameraRig> m_rig;
                std::shared_ptr<PowerUpTarget> m_target;
                Timer m_timer;
                bool m_inputRegistered = false;
                int m_cost;        
        };
}
