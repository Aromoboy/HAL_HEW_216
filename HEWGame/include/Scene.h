#ifndef __SCENE_H__
#define __SCENE_H__

#include "Core/UpdateScheduler.h"

class Scene : public Core::IUpdatable
{
public:
        Scene();
        virtual ~Scene();

        void RootUpdate(const Core::UpdateContext& context);
        void RootDraw();

        Core::UpdateNode* RegisterUpdatable(Core::IUpdatable* target, Core::UpdatePhase phase, int order = 0);
        void UnregisterUpdatable(Core::IUpdatable* target);
        void SetUpdatableOrder(Core::IUpdatable* target, int order);
        void SetUpdatableEnabled(Core::IUpdatable* target, bool enabled);
        Core::UpdateScheduler& GetScheduler();
        const Core::UpdateScheduler& GetScheduler() const;

        void OnUpdate(const Core::UpdateContext& context) override;

        virtual void Update(const Core::UpdateContext& context) = 0;
        virtual void Draw() = 0;

private:
        Core::UpdateScheduler m_scheduler;
};

#endif // __SCENE_H__
