#ifndef __SCENE_H__
#define __SCENE_H__

#include "Core/UpdateScheduler.h"

namespace Core
{
        struct UpdateContext;
}

namespace SceneSpace
{
        class Scene
        {
        public:
                void RootUpdate(Core::UpdateContext& ctx);
                void RootDraw();

                Scene();
                virtual ~Scene();
                virtual void Draw() = 0;

        protected:
                virtual void ConfigureUpdates();
                virtual void Update(Core::UpdateContext& ctx) = 0;

                Core::UpdateScheduler& GetUpdateScheduler();

        private:
                Core::UpdateScheduler m_scheduler;
                bool m_isConfigured;
        };
}

using Scene = SceneSpace::Scene;

#endif // __SCENE_H__
