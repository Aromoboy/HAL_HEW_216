// 選択画面のシーンをまとめている
// シーン管理から読み込まれて更新と描画を行う

#ifndef __SELECT_SCENE_H__
#define __SELECT_SCENE_H__

#include "Scene.h"
#include "SceneManager.h"

namespace CameraSpace
{
    class ICamera;
}

class SelectScene : public SceneSpace::Scene, public SceneSpace::IScene
{
public:
    SelectScene();

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
    bool m_inputRegistered = false;
};

#endif // __SELECT_SCENE_H__
