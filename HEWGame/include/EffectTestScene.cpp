#include "EffectTestScene.h"

#include "DirectX.h"
#include "Core/ActionId.h"
#include "Core/Input.h"

#include "Defines.h"
#include "PerspectiveCamera.h"
#include "TitleScene.h"

#include <string>


EffectTestScene::EffectTestScene(SceneSpace::SceneManager& manager)
        : m_manager(&manager)
{
}


void EffectTestScene::Draw()
{
        if (m_effectSystem != nullptr && m_camera != nullptr)
        {
                m_effectSystem->Draw(m_camera.get());
        }
}

void EffectTestScene::Update(Core::StepTimer& timer, Core::Input& input)
{
        RootUpdate(timer, input);
}

void EffectTestScene::Update(Core::UpdateContext& ctx)
{
        (void)ctx;
        Core::Input* input = GetInput();
        if (!input)
        {
                return;
        }

        if (input->IsTrigger(Core::ActionId::Cancel))
        {
                if (m_manager)
                {
                        m_manager->RequestSceneChange(std::make_unique<TitleScene>(*m_manager));
                }
                return;
        }
}



void EffectTestScene::Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
{
        (void)activeCamera;
        Draw();
}

bool EffectTestScene::Load(Core::Input& input)
{
        (void)input;

        m_effectMap.clear();
        m_effectMap.try_emplace("fly", EffectCreateData(u"Assets/Effect/fly_effect_test01.efkefc", 1.0f));

        if (!m_camera)
        {
                m_camera = std::make_shared<CameraSpace::PerspectiveCamera>();
        }
        return true;
}

void EffectTestScene::OnEnter()
{
        InitializeEffectSystem();

        auto registeredCamera = AddCamera(m_camera);
        if (registeredCamera)
        {
                SetActiveCamera(registeredCamera);
        }

        if (m_effectSystem != nullptr)
        {
                EffectSystem::EffectCreateRequest request(
                        "fly",
                        float3{ 0.0f, 0.0f, 5.0f },
                        float3{ 0.0f, 0.0f, 100.0f },
                        0);
                m_effectSystem->SpawnEffect(request);
        }
}

void EffectTestScene::OnExit()
{
        if (m_effectManager != nullptr)
        {
                m_effectManager->StopAllEffects();
        }

        if (m_effectSystem != nullptr)
        {
                Unregister(*m_effectSystem);
        }

        UnregisterSceneUpdateNode();
        UnregisterCameraUpdateNode();

        ShutdownEffectSystem();
}

void EffectTestScene::ConfigureUpdates()
{
        if (auto* input = GetInput())
        {
                Register(*input);
        }

        RegisterSceneUpdateNode(Core::UpdatePhase::Gameplay, 0);

        if (m_effectSystem != nullptr)
        {
                Register(*m_effectSystem);
        }
}

void EffectTestScene::InitializeEffectSystem()
{
        m_device = ::EffekseerRendererDX11::CreateGraphicsDevice(GetDevice(), GetContext());
        m_renderer = ::EffekseerRendererDX11::Renderer::Create(m_device, 8000);
        m_effectManager = Effekseer::Manager::Create(8000);

        if (m_renderer == nullptr || m_effectManager == nullptr)
        {
                ShutdownEffectSystem();
                return;
        }

        m_effectManager->SetSpriteRenderer(m_renderer->CreateSpriteRenderer());
        m_effectManager->SetRibbonRenderer(m_renderer->CreateRibbonRenderer());
        m_effectManager->SetRingRenderer(m_renderer->CreateRingRenderer());
        m_effectManager->SetTrackRenderer(m_renderer->CreateTrackRenderer());
        m_effectManager->SetModelRenderer(m_renderer->CreateModelRenderer());

        m_effectManager->SetTextureLoader(m_renderer->CreateTextureLoader());
        m_effectManager->SetModelLoader(m_renderer->CreateModelLoader());
        m_effectManager->SetMaterialLoader(m_renderer->CreateMaterialLoader());
        m_effectManager->SetCurveLoader(Effekseer::MakeRefPtr<Effekseer::CurveLoader>());

        m_effectSystem = std::make_unique<EffectSystem>(m_renderer, m_effectManager, m_effectMap);
}

void EffectTestScene::ShutdownEffectSystem()
{
        m_effectSystem.reset();
        m_effectManager.Reset();
        m_renderer.Reset();
        m_device.Reset();
}
