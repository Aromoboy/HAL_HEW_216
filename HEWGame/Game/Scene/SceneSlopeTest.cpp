#include "SceneSlopeTest.h"

#include "Core/ActionId.h"
#include "Core/Input.h"
#include "Core/StepTimer.h"
#include "Geometory.h"
#include "PerspectiveCamera.h"
#include "TitleScene.h"

#include <DirectXMath.h>
#include <cfloat>
#include <cmath>

using namespace DirectX;

namespace
{
    static float Length(const XMFLOAT3& v)
    {
        XMVECTOR vv = XMLoadFloat3(&v);
        float len = 0.0f;
        XMStoreFloat(&len, XMVector3Length(vv));
        return len;
    }

    static XMFLOAT3 Sub(const XMFLOAT3& a, const XMFLOAT3& b)
    {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    static XMFLOAT3 Add(const XMFLOAT3& a, const XMFLOAT3& b)
    {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }

    static XMFLOAT3 Mul(const XMFLOAT3& a, float s)
    {
        return { a.x * s, a.y * s, a.z * s };
    }

    static XMFLOAT4X4 ToFloat4x4(const XMMATRIX& matrix)
    {
        XMFLOAT4X4 out{};
        XMStoreFloat4x4(&out, matrix);
        return out;
    }

    static bool QueryBlockSurfaceHit(
        const Block& block,
        const XMFLOAT3& prevPos,
        const XMFLOAT3& moveVec,
        float radius,
        Collision::Result& outHitCenter,
        XMFLOAT3& outN,
        float& outT)
    {
        Collision::Plane p = block.GetPlane();

        XMVECTOR vN = XMLoadFloat3(&p.normal);
        vN = XMVector3Normalize(vN);
        XMStoreFloat3(&outN, vN);

        Collision::Plane pExp = p;
        pExp.normal = outN;
        pExp.pos = Add(p.pos, Mul(outN, radius));

        Collision::Ray r;
        r.start = prevPos;
        r.direction = moveVec;

        Collision::Result hit = Collision::Hit(pExp, r);
        if (!hit.isHit)
        {
            return false;
        }

        XMFLOAT3 surfacePoint = Sub(hit.point, Mul(outN, radius));

        bool inside = false;
        for (int i = 0; i < 2; ++i)
        {
            Collision::Triangle tri = block.GetTriangle(i);
            Collision::Result in = Collision::Hit(tri, surfacePoint);
            if (in.isHit)
            {
                inside = true;
                break;
            }
        }

        if (!inside)
        {
            return false;
        }

        outT = Length(Sub(hit.point, prevPos));
        outHitCenter = hit;
        return true;
    }
}

SceneSlopeTest::SceneSlopeTest(SceneSpace::SceneManager& manager)
    : m_manager(&manager)
{
}

bool SceneSlopeTest::Load(Core::Input& input)
{
    (void)input;

    m_pSlope = std::make_unique<Block>(XMFLOAT3{ 8.0f, 1.0f, 4.0f });
    m_pSlope->SetPos(XMFLOAT3{ -0.5f, 1.0f, 0.0f });
    m_pSlope->SetRotate(0.0f, 0.0f, -0.45f);

    m_pFloor = std::make_unique<Block>(XMFLOAT3{ 14.0f, 1.0f, 4.0f });
    m_pFloor->SetPos(XMFLOAT3{ 5.0f, -1.2f, 0.0f });
    m_pFloor->SetRotate(0.0f, 0.0f, 0.0f);

    m_ball.radius = 0.5f;
    m_ball.Reset(XMFLOAT3{ -3.0f, 6.0f, 0.0f });

    if (!m_camera)
    {
        m_camera = std::make_shared<CameraSpace::PerspectiveCamera>(45.0f);
    }

    return true;
}

void SceneSlopeTest::Update(Core::StepTimer& timer, Core::Input& input)
{
    RootUpdate(timer, input);
}

void SceneSlopeTest::Draw()
{
    RootDraw();
}

void SceneSlopeTest::OnEnter()
{
    if (!m_camera)
    {
        m_camera = std::make_shared<CameraSpace::PerspectiveCamera>(45.0f);
    }

    m_camera->SetPosition(float3{ 0.0f, 4.5f, -16.0f });
    m_camera->SetRotation(float3{ 0.2f, 0.0f, 0.0f });

    auto registeredCamera = AddCamera(m_camera);
    if (registeredCamera)
    {
        SetActiveCamera(registeredCamera);
    }
}

void SceneSlopeTest::OnExit()
{
    if (m_inputRegistered)
    {
        if (auto* input = GetInput())
        {
            Unregister(*input);
        }
        m_inputRegistered = false;
    }

    UnregisterSceneUpdateNode();
}

void SceneSlopeTest::ConfigureUpdates()
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
}

void SceneSlopeTest::Update(const Core::UpdateContext& ctx)
{
    Core::Input* input = GetInput();
    if (input && input->IsTrigger(Core::ActionId::Cancel))
    {
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<TitleScene>(*m_manager));
        }
        return;
    }

    const float dt = (ctx.dt > 0.0f) ? ctx.dt : (1.0f / 60.0f);

    const float g = 9.8f;
    const float groundDamping = 4.0f;
    const float stopEps = 0.03f;
    const float skin = 0.001f;

    m_ball.pos.z = 0.0f;
    m_ball.vel.z = 0.0f;

    XMFLOAT3 prevPos = m_ball.pos;

    m_ball.vel.y -= g * dt;
    m_ball.pos.x += m_ball.vel.x * dt;
    m_ball.pos.y += m_ball.vel.y * dt;
    m_ball.pos.z += m_ball.vel.z * dt;

    XMFLOAT3 moveVec = Sub(m_ball.pos, prevPos);

    bool hasHit = false;
    Collision::Result bestHit = {};
    XMFLOAT3 bestN = { 0.0f, 1.0f, 0.0f };
    float bestT = FLT_MAX;

    Collision::Result hit;
    XMFLOAT3 n;
    float t = 0.0f;

    if (m_pSlope && QueryBlockSurfaceHit(*m_pSlope, prevPos, moveVec, m_ball.radius, hit, n, t))
    {
        if (t < bestT)
        {
            bestT = t;
            bestHit = hit;
            bestN = n;
            hasHit = true;
        }
    }

    if (m_pFloor && QueryBlockSurfaceHit(*m_pFloor, prevPos, moveVec, m_ball.radius, hit, n, t))
    {
        if (t < bestT)
        {
            bestT = t;
            bestHit = hit;
            bestN = n;
            hasHit = true;
        }
    }

    if (hasHit)
    {
        m_ball.pos = Add(bestHit.point, Mul(bestN, skin));

        XMVECTOR vVel = XMLoadFloat3(&m_ball.vel);
        XMVECTOR vN = XMLoadFloat3(&bestN);

        float vn = 0.0f;
        XMStoreFloat(&vn, XMVector3Dot(vVel, vN));
        if (vn < 0.0f)
        {
            vVel = XMVectorSubtract(vVel, XMVectorScale(vN, vn));
        }

        float vn2 = 0.0f;
        XMStoreFloat(&vn2, XMVector3Dot(vVel, vN));
        XMVECTOR vT = XMVectorSubtract(vVel, XMVectorScale(vN, vn2));

        const float damp = std::exp(-groundDamping * dt);
        vT = XMVectorScale(vT, damp);

        XMStoreFloat3(&m_ball.vel, vT);

        if (std::fabs(m_ball.vel.x) < stopEps)
        {
            m_ball.vel.x = 0.0f;
        }
        if (std::fabs(m_ball.vel.y) < stopEps)
        {
            m_ball.vel.y = 0.0f;
        }
        if (std::fabs(m_ball.vel.z) < stopEps)
        {
            m_ball.vel.z = 0.0f;
        }
    }

    if (m_ball.pos.y < -100.0f)
    {
        m_ball.Reset(XMFLOAT3{ -3.0f, 6.0f, 0.0f });
    }
}

void SceneSlopeTest::Draw(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
{
    DrawWorld(activeCamera);
}

void SceneSlopeTest::DrawWorld(const std::shared_ptr<::CameraSpace::ICamera>& activeCamera)
{
    if (!activeCamera)
    {
        return;
    }

    Geometory::SetView(activeCamera->GetView());
    Geometory::SetProjection(activeCamera->GetProjection());

    if (m_pSlope)
    {
        m_pSlope->Draw();
    }

    if (m_pFloor)
    {
        m_pFloor->Draw();
    }

    XMMATRIX ballMat = XMMatrixScaling(m_ball.radius * 2.0f, m_ball.radius * 2.0f, m_ball.radius * 2.0f)
        * XMMatrixTranslation(m_ball.pos.x, m_ball.pos.y, m_ball.pos.z);
    Geometory::SetWorld(ToFloat4x4(ballMat));
    Geometory::DrawSphere();
}
