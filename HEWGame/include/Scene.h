#ifndef __SCENE_H__
#define __SCENE_H__

#include <chrono>
#include <cstdint>

#include "Core/UpdateScheduler.h"

namespace Core
{
        struct UpdateContext;
}

class Scene
{
public:
        void RootUpdate();
        void RootDraw();

        Scene();
        virtual ~Scene();
        virtual void Draw() = 0;

protected:
        virtual void ConfigureUpdates();
        virtual void Update(const Core::UpdateContext& ctx) = 0;

        Core::UpdateScheduler& GetUpdateScheduler();

private:
        Core::UpdateScheduler m_scheduler;
        bool m_isConfigured;
        std::uint64_t m_frameCounter;
        bool m_hasLastTick;
        std::chrono::steady_clock::time_point m_lastTick;
};

#endif // __SCENE_H__
