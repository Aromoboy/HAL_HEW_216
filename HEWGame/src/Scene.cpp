#include "Scene.h"

#include "Core/IUpdatable.h"
#include "Core/StepTimer.h"
#include "Core/Input.h"

namespace SceneSpace
{
        Scene::Scene()
                : m_isConfigured(false)
        {
        }

        Scene::~Scene()
        {
        }

        void Scene::RootUpdate(Core::StepTimer& timer, Core::Input& input)
        {
                if (!m_isConfigured)
                {
                        ConfigureUpdates();
                        m_isConfigured = true;
                }

                const float deltaTime = timer.Tick();
                Core::UpdateContext ctx(deltaTime, timer, input, this);

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
