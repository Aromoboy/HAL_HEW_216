// 画面切り替えをまとめた仕組みでゲーム起動中ずっと使う
// 現在のシーンと次に切り替えるシーンを保持して更新と描画を順番に呼ぶ
// 切り替え結果はゲーム全体の流れに反映される
#ifndef __SCENE_MANAGER_H__
#define __SCENE_MANAGER_H__

#include <memory>

namespace Core
{
    class StepTimer;
    class Input;
}

namespace SceneSpace
{
    class IScene
    {
    public:
        virtual ~IScene() = default;
        virtual bool Load(Core::Input& input) = 0;
        virtual void Update(Core::StepTimer& timer, Core::Input& input) = 0;
        virtual void Draw() = 0;
        virtual void OnEnter() {}
        virtual void OnExit() {}
    };

    class SceneManager
    {
    public:
        SceneManager(Core::StepTimer& timer, Core::Input& input);

        void SetInitialScene(std::unique_ptr<IScene> scene);
        void RequestSceneChange(std::unique_ptr<IScene> scene);
        void Update();
        void Draw();
        IScene* GetActiveScene();
        const IScene* GetActiveScene() const;

    private:
        void ActivatePendingScene();
        void LogSceneEvent(const char* eventLabel, const IScene* scene) const;

        Core::StepTimer& m_timer;
        Core::Input& m_input;
        std::unique_ptr<IScene> m_activeScene;
        std::unique_ptr<IScene> m_pendingScene;
        bool m_sceneLoaded;
        bool m_sceneEntered;
    };
}

#endif // __SCENE_MANAGER_H__
