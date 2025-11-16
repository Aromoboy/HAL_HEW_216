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

        m_gameCamera = std::make_shared<CameraSpace::PerspectiveCamera>(60.0f);
        m_gameCamera->SetPosition(float3{ 0.0f, 5.0f, -15.0f });
        m_gameCamera->SetRotation(float3{ 0.0f, 0.0f, 0.0f });

        auto registeredCamera = AddCamera(m_gameCamera);
        if (!registeredCamera)
        {
                DebugTrace(std::string(kLogPrefix) + "Failed to register gameplay camera.\n");
        }
        else
        {
                SetActiveCamera(registeredCamera);
        }

        if (m_gameCamera)
        {
                auto baseCamera = std::static_pointer_cast<CameraSpace::BaseCamera>(m_gameCamera);
                m_cameraRig = std::make_unique<CameraRig>(baseCamera);
                m_cameraRig->SetOffset(float3{ 0.0f, 3.0f, -12.0f });
                m_cameraRig->SetYaw(0.0f);
                m_cameraRig->SetPitch(-0.2f);
                m_cameraRig->SetFollowSpeed(6.0f);
                if (m_cameraAnchor)
                {
                        m_cameraRig->Attach(m_cameraAnchor);
                }
        }

        m_pManager = std::make_unique<SoundManagerEX>();

        m_loader = std::make_unique<AssimpModelLorder>();

        // 描画テスト用
        m_model = m_loader->Load("Assets/Model/LowPolyNature/Models/OiiaioooooiaiFin.fbx");

        m_render = std::make_unique<ModelRender>();
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
