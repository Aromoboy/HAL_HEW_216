#include "Scene.h"

#include "Core/IUpdatable.h"

namespace SceneSpace
{
        Scene::Scene()
                : m_isConfigured(false)
        {
        }

        Scene::~Scene()
        {
        }

        void Scene::RootUpdate(Core::UpdateContext& ctx)
        {
                if (!m_isConfigured)
                {
                        ConfigureUpdates();
                        m_isConfigured = true;
                }

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
}
