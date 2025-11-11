#include "Scene.h"

#include "Core/IUpdatable.h"

Scene::Scene()
        : m_isConfigured(false)
        , m_frameCounter(0)
        , m_hasLastTick(false)
{
}

Scene::~Scene()
{
}

void Scene::RootUpdate()
{
        if (!m_isConfigured)
        {
                ConfigureUpdates();
                m_isConfigured = true;
        }

        auto now = std::chrono::steady_clock::now();
        float deltaTime = 0.0f;
        if (m_hasLastTick)
        {
                deltaTime = std::chrono::duration<float>(now - m_lastTick).count();
        }
        else
        {
                m_hasLastTick = true;
        }
        m_lastTick = now;

        Core::UpdateContext ctx{};
        ctx.deltaTime = deltaTime;
        ctx.frameIndex = m_frameCounter++;
        ctx.scene = this;

        Update(ctx);
        m_scheduler.Tick(ctx);
}

void Scene::RootDraw()
{
        Draw();
}

void Scene::ConfigureUpdates()
{
}

Core::UpdateScheduler& Scene::GetUpdateScheduler()
{
        return m_scheduler;
}
