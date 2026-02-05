
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
#include "TitleRenderer.h"
#include "VideoBackgroundRenderer.h"

#include "Save/PlayerData.h"

namespace SceneSpace
{
    class SceneResult : public SceneSpace::Scene, public SceneSpace::IScene
    {
    public:
        explicit SceneResult(SceneManager& manager, int a_towerSize, int a_getCost);
        ~SceneResult() = default;

        bool Load(Core::Input& input) override;
        void Update(Core::StepTimer& timer, Core::Input& input) override;
        void Draw() override;

        void OnEnter() override;
        void OnExit() override;

        void ConfigureUpdates() override;
        void Update(const Core::UpdateContext& ctx) final;
        void Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera) final;
        void DrawOverlay() override;

    private:


        SceneManager* m_manager = nullptr;

        std::shared_ptr<CameraSpace::BaseCamera> m_camera;
        std::unique_ptr<CameraRig> m_rig;

        std::unique_ptr<ResultRenderer> m_resultRenderer;
        std::unique_ptr<ResultRenderer> m_renderer;
        std::array<std::shared_ptr<Texture2D>, 3> m_menuTextures;
        std::shared_ptr<Texture2D> m_menuSelectNodes;
        std::shared_ptr<Texture2D> m_backTexture;
        std::shared_ptr<Texture2D> m_pointAmount;
        std::shared_ptr<Texture2D> m_meter;

        int m_selectIndex = 0;
      
        int m_getCost;
        int m_towerSize;

        int m_cursorIndex = 0;
        static constexpr int kMenuCount = 3;
        bool m_inputRegistered = false;


        using VideoTexturePlayer = VideoBackgroundRenderer;
        std::unique_ptr<VideoTexturePlayer> m_videoPlayer;
        Texture m_texStartRed;
        Texture m_texStartGreen;
        Texture m_texPowerUpRed;
        Texture m_texPowerUpGreen;
        Texture m_texExitRed;
        Texture m_texExitGreen;
        Texture m_texStartEffect;
        Texture m_texNumber[4];


    };
}
