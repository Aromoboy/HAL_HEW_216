#ifndef __SCENE_H__
#define __SCENE_H__

#include "CameraManager.h"
#include "Core/UpdateScheduler.h"

namespace Core
{
        class StepTimer;
        class Input;
        struct UpdateContext;
        class IUpdatable;
}

namespace SceneSpace
{
class Scene
{
public:
        void RootUpdate(Core::StepTimer& timer, Core::Input& input);
        void RootDraw();

        Scene();
        virtual ~Scene();
        virtual void Draw() = 0;

protected:
        virtual void ConfigureUpdates();
        virtual void Update(Core::UpdateContext& ctx) = 0;

        void Register(Core::IUpdatable& node);
        void Unregister(Core::IUpdatable& node);

        Core::UpdateScheduler& GetUpdateScheduler();
        Core::Input* GetInput();
        const Core::Input* GetInput() const;
        CameraSpace::CameraManager& GetCameraManager();
        const CameraSpace::CameraManager& GetCameraManager() const;

private:
        Core::UpdateScheduler m_scheduler;
        bool m_isConfigured;
        Core::Input* m_input;
        CameraSpace::CameraManager m_cameraManager;
};
}

#endif // __SCENE_H__
