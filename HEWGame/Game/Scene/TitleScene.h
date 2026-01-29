#ifndef __TITLE_SCENE_H__
#define __TITLE_SCENE_H__

#include "Scene.h"
#include "SceneManager.h"
#include "VideoTexturePlayer.h"
#include <array> 
#include <memory>
#include "TitleRenderer.h"
#include "Title/TitleManager.h"
#include "PowerUpTarget.h"
#include "CameraRig.h"


namespace CameraSpace
{
    class ICamera;
    class PerspectiveCamera;
}

class TitleScene : public SceneSpace::Scene, public SceneSpace::IScene
{
    enum class UiState
    {
        MainMenu,
        Setting,
    };

    enum class TitleMenu
    {
        Start,
        PowerUp,
        Exit,
        Max
    };

public:
    explicit TitleScene(SceneSpace::SceneManager& manager);

    bool Load(Core::Input& input) override;
    void Update(Core::StepTimer& timer, Core::Input& input) override;
    void Draw() override;
    void OnEnter() override;
    void OnExit() override;

protected:
    void ConfigureUpdates() override;
    void Update(const Core::UpdateContext& ctx) override;
    void Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera) override;

private:
    SceneSpace::SceneManager* m_manager;
    std::shared_ptr<CameraSpace::PerspectiveCamera> m_camera;
    bool m_inputRegistered = false;
    TextureCache* m_textureCache = nullptr;

private:
    UiState m_uiState = UiState::MainMenu;
    int m_cursor = 0;
    std::unique_ptr<VideoTexturePlayer> m_video;

    std::unique_ptr<TitleRenderer> m_renderer;
    std::unique_ptr<TitleManager> m_titleManager;

    std::unique_ptr<CameraRig> m_rig;
    std::shared_ptr<PowerUpTarget> m_target;


    int m_titleBGMHandle = -1;
    bool m_shouldStopBGM = true;
};

#endif
