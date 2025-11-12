#ifndef __SCENE_H__
#define __SCENE_H__

#include "Core/UpdateScheduler.h"

namespace Core
{
        class StepTimer;
        class Input;
        struct UpdateContext;
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

        Core::UpdateScheduler& GetUpdateScheduler();
        Core::Input* GetInput();
        const Core::Input* GetInput() const;

private:
        Core::UpdateScheduler m_scheduler;
        bool m_isConfigured;
        Core::Input* m_input;
};
}

#endif // __SCENE_H__
