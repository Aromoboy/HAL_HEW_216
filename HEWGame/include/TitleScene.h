#ifndef __TITLE_SCENE_H__
#define __TITLE_SCENE_H__

#include "Scene.h"
#include "SceneManager.h"

class TitleScene : public SceneSpace::Scene, public SceneSpace::IScene
{
public:
    explicit TitleScene(SceneSpace::SceneManager& manager);

    void Load(Core::Input& input) override;
    void Update(Core::StepTimer& timer, Core::Input& input) override;
    void Draw() override;
    void OnEnter() override;
    void OnExit() override;

protected:
    void ConfigureUpdates() override;
    void Update(Core::UpdateContext& ctx) override;
    void Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera) override;

private:
    void LogLifecycle(const char* label) const;

    SceneSpace::SceneManager* m_manager;
};

#endif // __TITLE_SCENE_H__
