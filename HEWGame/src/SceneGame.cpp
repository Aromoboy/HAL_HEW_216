#include "SceneGame.h"
#include "Geometory.h"
#include "DrawMatrix.h"
#include "Vector.h"
#include "ShaderList.h"
#include "Defines.h"
#include "DebugConsole.h"
#include "AssimpModelLorder.h"
#include "Core/Input.h"
#include "Systems/PhysicsMachine.h"

#include <sstream>
#include <string>

#if defined(_WIN32)
#       ifndef NOMINMAX
#               define NOMINMAX
#       endif
#       include <windows.h>
#endif

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
        void DebugTrace(const std::string& message)
        {
                ::OutputDebugStringA(message.c_str());
        }
#else
        void DebugTrace(const std::string& message)
        {
                (void)message;
        }
#endif

constexpr const char* kLogPrefix = "[SceneGame] ";

// Designers can flip this flag (or expose it via a console binding) to evaluate
// the fixed-position prototype camera without rewiring the rig itself.
constexpr bool kForceFixedCameraPrototype = false;

constexpr CameraPrototypeSettings kFollowCameraPrototype{
false,
6.0f,
{ 0.0f, 3.0f, -12.0f },
60.0f,
0.0f,
-0.2f,
};

constexpr CameraPrototypeSettings kFixedCameraPrototype{
true,
4.0f,
{ 0.0f, 7.0f, -18.0f },
50.0f,
0.05f,
-0.35f,
};

const CameraPrototypeSettings& GetActiveCameraPrototypeSettings()
{
return kForceFixedCameraPrototype ? kFixedCameraPrototype : kFollowCameraPrototype;
}
}

Model::AnimeNo no;
float3 fRads = { 0.0f, 0.0f, 0.0f };

SceneGame::SceneGame()
{
        m_physicsMachine = std::make_unique<Systems::PhysicsMachine>();

        RenderTarget* pRTV = GetDefaultRTV();
        DepthStencil* pDSV = GetDefaultDSV();
        SetRenderTargets(1, &pRTV, pDSV);
        SetDepthTest(true);
        m_pModel = std::make_unique<Model>();
        if (!m_pModel->Load("Assets/Model/LowPolyNature/Models/Flowers_01.fbx", 1.0f, Model::Flip::None))
        {
                no = m_pModel->AddAnimation("Assets/Model/LowPolyNature/Models/BoxsLam.fbx");
        }

        no = m_pModel->AddAnimation("Assets/Model/LowPolyNature/BoxsLam.fbx");

        m_pModel->Play(no, 1, 1.0f);

        for (int i = 0; i < 9; i++)
        {
                m_pObjects.push_back(std::make_shared<TestObject>("OiiaioooooiaiFin", float3{ -2.0f ,0.0f ,-5.0f }, float3{ 1.0f ,1.0f,1.0f }, float3{ 0.02f * i, 0.0f, 0.0f }));
        }

        m_cameraAnchor = std::make_shared<SceneSpace::Entity>("PlayerAnchor");
        if (!m_pObjects.empty())
        {
                auto& anchorTransform = m_cameraAnchor->GetTransform();
                anchorTransform.position = m_pObjects.front()->GetPosition();
                anchorTransform.rotation = m_pObjects.front()->GetfRads();
                anchorTransform.scale = m_pObjects.front()->GetScale();
        }

        ConfigureCamera(GetActiveCameraPrototypeSettings());

        m_pManager = std::make_unique<SoundManagerEX>();

        m_loader = std::make_unique<AssimpModelLorder>();

        // 描画テスト用
        m_model = m_loader->Load("Assets/Model/LowPolyNature/Models/OiiaioooooiaiFin.fbx");

        m_render = std::make_unique<ModelRender>();
}

void SceneGame::ConfigureCamera(const CameraPrototypeSettings& settings)
{
        m_gameCamera = std::make_shared<CameraSpace::PerspectiveCamera>(settings.fovDegrees);
        if (!m_gameCamera)
        {
                DebugTrace(std::string(kLogPrefix) + "ConfigureCamera failed: perspective camera allocation returned null.\n");
                return;
        }

        m_gameCamera->SetPosition(float3{ 0.0f, 5.0f, -15.0f });
        m_gameCamera->SetRotation(float3{ 0.0f, 0.0f, 0.0f });

        auto registeredCamera = AddCamera(m_gameCamera);
        if (!registeredCamera)
        {
                DebugTrace(std::string(kLogPrefix) + "ConfigureCamera failed: registration with SceneManager was rejected.\n");
        }
        else
        {
                SetActiveCamera(registeredCamera);
        }

        auto baseCamera = std::static_pointer_cast<CameraSpace::BaseCamera>(m_gameCamera);
        m_cameraRig = std::make_unique<CameraRig>(baseCamera);
        if (!m_cameraRig)
        {
                DebugTrace(std::string(kLogPrefix) + "ConfigureCamera failed: CameraRig allocation returned null.\n");
                return;
        }

        m_cameraRig->SetOffset(settings.offset);
        m_cameraRig->SetYaw(settings.yawRadians);
        m_cameraRig->SetPitch(settings.pitchRadians);
        m_cameraRig->SetFollowSpeed(settings.fixedPosition ? 0.0f : settings.followSpeed);
        m_cameraRig->SetTranslationLocked(settings.fixedPosition);
        m_cameraRig->SetYawPitchLocked(false);
        if (m_cameraAnchor)
        {
                m_cameraRig->Attach(m_cameraAnchor);
        }

        DebugTrace(std::string(kLogPrefix) + "Camera rig configured as " + (settings.fixedPosition ? "fixed" : "follow") + " prototype.\n");
}

SceneGame::~SceneGame()
{
        if (m_inputRegistered)
        {
                if (auto* input = GetInput())
                {
                        Unregister(*input);
                }
                m_inputRegistered = false;
        }

        if (m_physicsRegistered && m_physicsMachine)
        {
                GetUpdateScheduler().Unregister(*m_physicsMachine);
                m_physicsRegistered = false;
        }

        if (m_cameraRigRegistered && m_cameraRig)
        {
                Unregister(*m_cameraRig);
                m_cameraRigRegistered = false;
        }
}

void SceneGame::ConfigureUpdates()
{
        if (auto* input = GetInput())
        {
                Register(*input);
                m_inputRegistered = true;
        }

        if (m_physicsMachine && !m_physicsRegistered)
        {
                GetUpdateScheduler().Register(*m_physicsMachine);
                m_physicsRegistered = true;
        }

        if (m_cameraRig && !m_cameraRigRegistered)
        {
                Register(*m_cameraRig);
                m_cameraRigRegistered = true;
        }
}

void SceneGame::Update(Core::UpdateContext& ctx)
{
        (void)ctx;
        fRads.z += 0.001f * RAD;

        if (m_cameraAnchor && !m_pObjects.empty())
        {
                auto& anchorTransform = m_cameraAnchor->GetTransform();
                anchorTransform.position = m_pObjects.front()->GetPosition();
                anchorTransform.rotation = m_pObjects.front()->GetfRads();
                anchorTransform.scale = m_pObjects.front()->GetScale();
        }
}

void SceneGame::Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera)
{
        // 描画確認用の数値
        if (!activeCamera)
        {
                DebugTrace(std::string(kLogPrefix) + "Draw skipped: no active camera registered.\n");
                return;
        }

        Geometory::SetView(activeCamera->GetView());
        Geometory::SetProjection(activeCamera->GetProjection());

        if (!m_render || !m_model)
        {
                DebugTrace(std::string(kLogPrefix) + "Draw skipped: renderer or model missing.\n");
                return;
        }

        m_render->Draw(*m_model, activeCamera.get());
}
