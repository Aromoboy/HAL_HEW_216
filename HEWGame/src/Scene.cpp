#include "Scene.h"

Scene::Scene()
{
        m_scheduler.Register(this, Core::UpdatePhase::Simulation);
}

Scene::~Scene()
{
        m_scheduler.Unregister(this);
}

void Scene::RootUpdate(const Core::UpdateContext& context)
{
        m_scheduler.Tick(context);
}

void Scene::RootDraw()
{
        Draw();
}

Core::UpdateNode* Scene::RegisterUpdatable(Core::IUpdatable* target, Core::UpdatePhase phase, int order)
{
        return m_scheduler.Register(target, phase, order);
}

void Scene::UnregisterUpdatable(Core::IUpdatable* target)
{
        m_scheduler.Unregister(target);
}

void Scene::SetUpdatableOrder(Core::IUpdatable* target, int order)
{
        m_scheduler.SetOrder(target, order);
}

void Scene::SetUpdatableEnabled(Core::IUpdatable* target, bool enabled)
{
        m_scheduler.SetEnabled(target, enabled);
}

Core::UpdateScheduler& Scene::GetScheduler()
{
        return m_scheduler;
}

const Core::UpdateScheduler& Scene::GetScheduler() const
{
        return m_scheduler;
}

void Scene::OnUpdate(const Core::UpdateContext& context)
{
        Update(context);
}
