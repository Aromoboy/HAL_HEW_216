
#pragma once

#include <memory>
#include <array>

#include "Scene.h"
#include "SceneManager.h"
#include "BaseCamera.h"
#include "CameraRig.h"
#include "ResultRenderer.h"
#include "Sprite.h"
#include "Texture.h"
#include "BackgroundRenderer.h"
#include "TitleRenderer.h"


namespace SceneSpace
{
    class SceneResult : public SceneSpace::Scene, public SceneSpace::IScene
    {
    public:
        explicit SceneResult(SceneManager& manager);
        ~SceneResult() = default;

        bool Load(Core::Input& input) override;
        void Update(Core::StepTimer& timer, Core::Input& input) override;
        void Draw() override;

        void OnEnter() override;
        void OnExit() override;

        void ConfigureUpdates() override;
        void Update(const Core::UpdateContext& ctx) final;
        void Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera) final;

    private:
        SceneManager* m_manager = nullptr;

        std::shared_ptr<CameraSpace::BaseCamera> m_camera;
        std::unique_ptr<CameraRig> m_rig;

        std::unique_ptr<ResultRenderer> m_resultRenderer;
        std::unique_ptr<ResultRenderer> m_renderer;
        std::array<std::unique_ptr<Texture>, 3> m_menuTextures;
       

        int m_selectIndex = 0;
      

        int m_cursorIndex = 0;
        static constexpr int kMenuCount = 3;
        bool m_inputRegistered = false;


        std::unique_ptr<BackgroundRenderer> m_background;
        Texture m_texStartRed;
        Texture m_texStartGreen;
        Texture m_texPowerUpRed;
        Texture m_texPowerUpGreen;
        Texture m_texExitRed;
        Texture m_texExitGreen;
        Texture m_texStartEffect;
        Texture m_texNumber[4];

        std::shared_ptr<Texture2D> m_backTexture;

    };
}

