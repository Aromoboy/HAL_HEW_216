#include "Scene.h"

#include "Core/IUpdatable.h"
#include "Core/StepTimer.h"
#include "Core/Input.h"

namespace SceneSpace
{
        Scene::Scene()
                : m_isConfigured(false)
                , m_input(nullptr)
        {
        }

        Scene::~Scene()
        {
        }

        void Scene::RootUpdate(Core::StepTimer& timer, Core::Input& input)
        {
                m_input = &input;

                if (!m_isConfigured)
                {
                        ConfigureUpdates();
                        m_isConfigured = true;
                }

                const float deltaTime = timer.DeltaSeconds();
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

        Core::Input* Scene::GetInput()
        {
                return m_input;
        }

        const Core::Input* Scene::GetInput() const
        {
                return m_input;
        }
}
