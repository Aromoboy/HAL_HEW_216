#ifndef __TITLE_SCENE_H__
#define __TITLE_SCENE_H__

#include "Scene.h"
#include "SceneManager.h"
#include "BackgroundRenderer.h"
#include <array> 
#include <memory>
#include "TitleRenderer.h"



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

    enum class TitleState
    {
        Idle,       // タイトル待機
        CountDown,  // 暗転＋カウントダウン
        StartEffect // START表示
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
    TitleState m_titleState = TitleState::Idle;
    int m_count = 3;
    float m_countTimer = 0.0f;
    float m_startTimer = 0.0f;
    float m_deltaTime = 0.0f;
    float m_fadeAlpha = 0;
    Texture m_texStartRed;
    Texture m_texStartGreen;
    Texture m_texPowerUpRed;
    Texture m_texPowerUpGreen;
    Texture m_texExitRed;
    Texture m_texExitGreen;
    Texture m_texStartEffect;

    struct MenuItem
    {
        std::unique_ptr<TitleRenderer> red;
        std::unique_ptr<TitleRenderer> green;
    };
    std::array<Texture, 4> m_texNumber;
    std::array<MenuItem, static_cast<int>(TitleMenu::Max)> m_menu;
    std::unique_ptr<BackgroundRenderer> m_background;

    int m_titleBGMHandle = -1;
};

#endif
