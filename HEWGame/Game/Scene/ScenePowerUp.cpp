#include "ScenePowerUp.h"

#include "PerspectiveCamera.h"
#include "TitleScene.h"

#include "Core/Input.h"
#include "Core/StepTimer.h"

namespace SceneSpace
{
        ScenePowerUp::ScenePowerUp(SceneSpace::SceneManager& manager)
                : m_manager(&manager)
        {
        }

        bool ScenePowerUp::Load(Core::Input& input)
        {
                (void)input;

                m_camera = std::make_shared<CameraSpace::PerspectiveCamera>(45.0f);
                m_rig = std::make_unique<CameraRig>(m_camera);
                return true;
        }

        void ScenePowerUp::Update(Core::StepTimer& timer, Core::Input& input)
        {
                RootUpdate(timer, input);
        }

        void ScenePowerUp::Draw()
        {
            SetDepthTest(false);
                RootDraw();
        }

        void ScenePowerUp::OnEnter()
        {
                if (!m_camera)
                {
                        return;
                }

                auto registeredCamera = AddCamera(m_camera);

                if (registeredCamera)
                {
                        SetActiveCamera(registeredCamera);
                }
        }

        void ScenePowerUp::OnExit()
        {
                if (m_inputRegistered)
                {
                        if (auto* input = GetInput())
                        {
                                Unregister(*input);
                        }
                        m_inputRegistered = false;
                }
                if (m_rig)
                {
                        Unregister(*m_rig);
                        m_rig.reset();
                }
                m_camera.reset();
                UnregisterSceneUpdateNode();
                UnregisterCameraUpdateNode();
        }

        void  ScenePowerUp::ConfigureUpdates()
        {
                if (!m_inputRegistered)
                {
                        if (auto* input = GetInput())
                        {
                                if (Register(*input))
                                {
                                        m_inputRegistered = true;
                                }
                        }
                }

                RegisterSceneUpdateNode(Core::UpdatePhase::Gameplay, 0);

                if (m_rig)
                {
                        Register(*m_rig);
                }

        }

        void ScenePowerUp::Update(const Core::UpdateContext& ctx)
        {
                (void)ctx;
                Core::Input* input = GetInput();
                if (input && input->IsTrigger(Core::ActionId::Cancel))
                {
                        if (m_manager)
                        {
                                m_manager->RequestSceneChange(std::make_unique<TitleScene>(*m_manager));
                        }

                        return;
                }

        }

        void ScenePowerUp::Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera)
        {
                (void)activeCamera;
        }
}
