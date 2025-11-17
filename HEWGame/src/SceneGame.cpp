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
#include "AABBCollider.h"
#include "ModelCache.h"



#include <array>
#include <sstream>
#include <string>
#include <DirectXMath.h>
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
        constexpr size_t kDefaultCameraPrototypeIndex = 0;
        constexpr int kCameraPrototypeConsoleOverride = -1; // Change to >=0 to force a prototype at startup.

        DirectX::XMVECTOR LoadFloat3(const float3& value)
        {
            return DirectX::XMVectorSet(value.x, value.y, value.z, 0.0f);
        }

        float3 StoreFloat3(DirectX::FXMVECTOR value)
        {
            DirectX::XMFLOAT3 temp;
            DirectX::XMStoreFloat3(&temp, value);
            return float3{ temp.x, temp.y, temp.z };
        }

        DirectX::XMVECTOR RotateOffset(const float3& offset, float yaw, float pitch)
        {
            DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, 0.0f);
            return DirectX::XMVector3Transform(LoadFloat3(offset), rotation);
        }

        const std::array<CameraPrototypeSettings, 2> kCameraPrototypes =
        { {
                CameraPrototypeSettings
                {
                        false,
                        6.0f,
                        float3{ 0.0f, 3.0f, -12.0f },
                        60.0f,
                        0.0f,
                        -0.2f
                },
                CameraPrototypeSettings
                {
                        true,
                        4.0f,
                        float3{ 0.0f, 8.0f, -22.0f },
                        45.0f,
                        0.0f,
                        -0.35f
                }
        } };

        size_t NormalizeCameraPrototypeIndex(size_t requestedIndex)
        {
            if (requestedIndex >= kCameraPrototypes.size())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Camera prototype index " << requestedIndex
                    << " is invalid. Falling back to index " << kDefaultCameraPrototypeIndex << "\n";
                DebugTrace(oss.str());
                return kDefaultCameraPrototypeIndex;
            }
            return requestedIndex;
        }

        size_t DetermineStartupCameraPrototypeIndex()
        {
            size_t requestedIndex = kDefaultCameraPrototypeIndex;
            if (kCameraPrototypeConsoleOverride >= 0)
            {
                requestedIndex = static_cast<size_t>(kCameraPrototypeConsoleOverride);
            }
            return NormalizeCameraPrototypeIndex(requestedIndex);
        }
}

Model::AnimeNo no;
float3 fRads = { 0.0f, 0.0f, 0.0f };

SceneGame::SceneGame()
    : m_activeCameraPrototypeIndex(DetermineStartupCameraPrototypeIndex())
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

    const auto& cameraSettings = kCameraPrototypes[m_activeCameraPrototypeIndex];
    m_gameCamera = std::make_shared<CameraSpace::PerspectiveCamera>(cameraSettings.fovDegrees);
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
        ConfigureCamera(cameraSettings);
        if (m_cameraAnchor)
        {
            m_cameraRig->Attach(m_cameraAnchor);
        }
    }

    m_pManager = std::make_unique<SoundManagerEX>();

    m_loader = std::make_shared<AssimpModelLorder>();

    Render::ModelCache& cache = Render::ModelCache::Instance();
    cache.SetLoader(m_loader);

    // 描画テスト用
    m_model = m_loader->Load("Assets/Model/LowPolyNature/Models/OiiaioooooiaiFin.fbx");

    
    m_render = std::make_unique<ModelRender>();
    std::shared_ptr<Collision::ICollider> collider;
    Transform trans =
    { float3{0.0f, 0.0f, 0.0f}, float3{0.0f, 0.0f, 0.0f}, float3{1.0f, 1.0f, 1.0f} };
    collider = std::make_shared<Collision::AABBCollider>(trans, false, 1.0f, float3{ 0.0f, 0.0f, 0.0f });
    player = std::make_unique<SceneSpace::Player>("player", float3{ 0.0f, 0.0f, 0.0f }, float3{ 0.0f, 0.0f, 0.0f }, float3{ 1.0f, 1.0f, 1.0f }, collider);

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

        if (player)
        {
                Unregister(*player);
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

        if (player)
        {
            Register(*player);
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

        player->Draw(*m_render, *activeCamera.get());
        //m_render->Draw(*m_model, activeCamera.get());
}

void SceneGame::ApplyCameraPrototype(size_t index)
{
    const size_t normalizedIndex = NormalizeCameraPrototypeIndex(index);
    m_activeCameraPrototypeIndex = normalizedIndex;
    ConfigureCamera(kCameraPrototypes[m_activeCameraPrototypeIndex]);

    const auto& settings = kCameraPrototypes[m_activeCameraPrototypeIndex];
    std::ostringstream oss;
    oss << kLogPrefix << "Camera prototype switched to index " << m_activeCameraPrototypeIndex
        << " (fixed=" << std::boolalpha << settings.fixedPosition
        << ", fov=" << settings.fovDegrees << ", followSpeed=" << settings.followSpeed << ")\n";
    DebugTrace(oss.str());
}

void SceneGame::ConfigureCamera(const CameraPrototypeSettings& settings)
{
    if (m_gameCamera)
    {
        m_gameCamera->SetFieldOfViewDegrees(settings.fovDegrees);
    }

    if (m_cameraRig)
    {
        m_cameraRig->SetOffset(settings.offset);
        m_cameraRig->SetYaw(settings.yawRadians);
        m_cameraRig->SetPitch(settings.pitchRadians);
        m_cameraRig->SetFollowSpeed(settings.fixedPosition ? 0.0f : settings.followSpeed);
        m_cameraRig->SetTranslationLocked(settings.fixedPosition);
        m_cameraRig->SetYawPitchLocked(settings.fixedPosition);
    }

    if (!m_gameCamera)
    {
        DebugTrace(std::string(kLogPrefix) + "ConfigureCamera skipped: gameplay camera missing.\n");
        return;
    }

    float3 cameraRotation{ settings.pitchRadians, settings.yawRadians, 0.0f };
    if (m_cameraAnchor)
    {
        const auto& anchorTransform = m_cameraAnchor->GetTransform();
        cameraRotation.x += anchorTransform.rotation.x;
        cameraRotation.y += anchorTransform.rotation.y;
        cameraRotation.z += anchorTransform.rotation.z;

        DirectX::XMVECTOR anchorPos = LoadFloat3(anchorTransform.position);
        DirectX::XMVECTOR rotatedOffset = RotateOffset(settings.offset, cameraRotation.y, cameraRotation.x);
        DirectX::XMVECTOR desiredPos = DirectX::XMVectorAdd(anchorPos, rotatedOffset);
        m_gameCamera->SetPosition(StoreFloat3(desiredPos));
    }
    else
    {
        m_gameCamera->SetPosition(settings.offset);
    }

    m_gameCamera->SetRotation(cameraRotation);
// BaseCamera::RecalculateViewを直接呼び出す代わりに、パブリック API 経由でビューの更新を強制
// BaseCamera::RecalculateViewは保護されたメソッドです。GetView はトランスフォームが変更された際に遅延評価で再計算
    (void)m_gameCamera->GetView();

    std::ostringstream oss;
    oss << kLogPrefix << "Configured camera prototype: fixed=" << std::boolalpha << settings.fixedPosition
        << ", fov=" << settings.fovDegrees
        << ", followSpeed=" << settings.followSpeed
        << ", offset=" << settings.offset.x << ',' << settings.offset.y << ',' << settings.offset.z
        << ", yaw=" << settings.yawRadians << ", pitch=" << settings.pitchRadians << "\n";
    DebugTrace(oss.str());
}