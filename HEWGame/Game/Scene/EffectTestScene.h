#pragma once


#include <Effekseer.h>
#include <EffekseerRendererDX11.h>
#include <EffekseerSoundXAudio2.h>
#include <memory>
#include <unordered_map>
#include "ICamera.h"
#include "Scene.h"
#include "SceneManager.h"
#include "EffectSystem.h"


class EffectTestScene :public SceneSpace::Scene, public SceneSpace::IScene
{
public:
        explicit EffectTestScene(SceneSpace::SceneManager& manager);
        ~EffectTestScene() override = default;

        void Update(Core::StepTimer& timer, Core::Input& input) override;
        void Update(const Core::UpdateContext& ctx) override;
        void Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera) override;
        void Draw() override;
        bool Load(Core::Input& input) override;
        void OnEnter() override;
        void OnExit() override;
        void ConfigureUpdates() override;
private:
        void InitializeEffectSystem();
        void ShutdownEffectSystem();

        SceneSpace::SceneManager* m_manager;

        std::unique_ptr<EffectSystem> m_effectSystem;

        std::shared_ptr<CameraSpace::ICamera> m_camera;
        ::Effekseer::Backend::GraphicsDeviceRef m_device;
        ::EffekseerRendererDX11::RendererRef m_renderer;
        ::Effekseer::ManagerRef m_effectManager;
        std::unordered_map<std::string, EffectCreateData> m_effectMap;
        bool m_inputRegistered = false;
};
