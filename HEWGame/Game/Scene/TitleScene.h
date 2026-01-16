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
    std::unique_ptr<BackgroundRenderer> m_background;
    bool m_inputRegistered = false;
    TextureCache* m_textureCache = nullptr;
};

#endif
