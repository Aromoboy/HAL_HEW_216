#pragma once
//Setting.h
#ifndef __SETTING_SCENE_H__
#define __SETTING_SCENE_H__

#include "Scene.h"
#include "SceneManager.h"
#include "BackgroundRenderer.h"

#include <memory>


namespace CameraSpace
{
    class ICamera;
    class PerspectiveCamera;
}


namespace SceneSpace
{
    class SettingScene: public SceneSpace::Scene, public SceneSpace::IScene
    {

    public:

        explicit SettingScene(SceneSpace::SceneManager& manager);

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
        SceneManager* m_manager = nullptr;
        std::shared_ptr<CameraSpace::PerspectiveCamera> m_camera;
        std::unique_ptr<BackgroundRenderer> m_background;

   
    };
}
#endif // __SETTING_SCENE_H__
