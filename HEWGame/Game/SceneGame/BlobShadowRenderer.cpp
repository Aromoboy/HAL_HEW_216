#include "BlobShadowRenderer.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

#include "DirectX.h"
#include "DrawMatrix.h"
#include "MeshDX.h"
#include "PerspectiveCamera.h"
#include "Shader.h"
#include "Vector.h"

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
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

    constexpr float kReceiverZFallbackMultiplier = 2.0f;

    bool IsFiniteFloat(float value)
    {
        return std::isfinite(value) != 0;
    }

    bool IsFiniteVector(const float3& value)
    {
        return IsFiniteFloat(value.x) && IsFiniteFloat(value.y) && IsFiniteFloat(value.z);
    }

    float SafeSize(float value, float fallback)
    {
        if (!IsFiniteFloat(value) || value <= 0.0f)
        {
            return fallback;
        }

        return value;
    }
}

BlobShadowRenderer::BlobShadowRenderer() = default;

BlobShadowRenderer::~BlobShadowRenderer() = default;

void BlobShadowRenderer::SetSettings(const SceneGameConfig::BlobShadowSettings& settings)
{
    m_settings = settings;
}

bool BlobShadowRenderer::EnsureReady()
{
    if (m_ready)
    {
        return true;
    }

    const char* vsCode = R"EOT(
struct VS_IN {
    float3 pos : POSITION0;
    float2 uv : TEXCOORD0;
};
struct VS_OUT {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};
cbuffer Matrix : register(b0) {
    float4x4 world;
    float4x4 view;
    float4x4 proj;
};
VS_OUT main(VS_IN vin) {
    VS_OUT vout;
    float4 p = float4(vin.pos, 1.0f);
    p = mul(p, world);
    p = mul(p, view);
    p = mul(p, proj);
    vout.pos = p;
    vout.uv = vin.uv;
    return vout;
})EOT";

    const char* psCode = R"EOT(
struct PS_IN {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};
cbuffer Param : register(b0) {
    float4 color;
    float softness;
    float3 padding;
};
float4 main(PS_IN pin) : SV_TARGET {
    float2 centered = pin.uv * 2.0f - 1.0f;
    float dist = length(centered);
    float soft = max(softness, 1.0e-4f);
    float edge = 1.0f - smoothstep(1.0f - soft, 1.0f, dist);
    float alpha = color.a * edge;
    return float4(color.rgb, alpha);
})EOT";

    struct Vertex
    {
        float pos[3];
        float uv[2];
    } vertices[] = {
        {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f}},
        {{ 0.5f, 0.5f, 0.0f}, {1.0f, 0.0f}},
        {{-0.5f,-0.5f, 0.0f}, {0.0f, 1.0f}},
        {{ 0.5f,-0.5f, 0.0f}, {1.0f, 1.0f}},
    };

    MeshDX::Description desc{};
    desc.pVtx = vertices;
    desc.vtxSize = sizeof(Vertex);
    desc.vtxCount = 4;
    desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

    m_mesh = std::make_shared<MeshDX>(desc, -1);
    m_vs = std::make_shared<VertexShader>();
    m_vs->Compile(vsCode);
    m_ps = std::make_shared<PixelShader>();
    m_ps->Compile(psCode);

    m_ready = static_cast<bool>(m_mesh) && static_cast<bool>(m_vs) && static_cast<bool>(m_ps);
    return m_ready;
}

void BlobShadowRenderer::DrawShadow(Entities::PhysicalEntity& entity,
    const CameraSpace::ICamera& camera,
    float receiverY,
    std::optional<float> receiverZ,
    std::optional<float> receiverHeight)
{
    if (!m_settings.enable || !EnsureReady())
    {
        return;
    }

    entity.SyncColliderBounds();
    const AABB& bounds = entity.GetAABB();
    const Transform renderTransform = entity.GetRenderTransform();
    if (!IsFiniteVector(renderTransform.position))
    {
        return;
    }

    float width = SafeSize(bounds.max.x - bounds.min.x, 1.0f);
    float depth = SafeSize(bounds.max.z - bounds.min.z, width);
    depth = SafeSize(depth, width);

    float height = renderTransform.position.y - receiverY;
    if (receiverHeight && IsFiniteFloat(*receiverHeight))
    {
        height = *receiverHeight;
    }
    if (!IsFiniteFloat(height))
    {
        return;
    }
    height = std::max(0.0f, height);
    float heightRatio = 0.0f;
    if (m_settings.maxHeight > 0.0f)
    {
        heightRatio = std::clamp(height / m_settings.maxHeight, 0.0f, 1.0f);
    }
    else
    {
        heightRatio = 1.0f;
    }

    const float scaleFactor = std::max(0.0f, std::lerp(1.0f, m_settings.minScale, heightRatio));
    const float alpha = std::max(0.0f, m_settings.opacity * (1.0f - heightRatio));
    if (alpha <= 0.0f || scaleFactor <= 0.0f)
    {
        return;
    }

    const float casterZ = renderTransform.position.z;
    const float receiverThreshold = std::max(width, depth) * kReceiverZFallbackMultiplier;
    const bool receiverZValid = receiverZ && IsFiniteFloat(*receiverZ);
    const bool receiverZAligned = receiverZValid
        && std::abs(*receiverZ - casterZ) <= receiverThreshold;
    const float resolvedReceiverZ = (receiverZValid && m_settings.forceReceiverZ)
        ? *receiverZ
        : (receiverZAligned ? *receiverZ : casterZ);

    if (m_debugLogRemaining > 0)
    {
        std::optional<float> cameraNear;
        std::optional<float> cameraFar;
        if (const auto* perspectiveCamera = dynamic_cast<const CameraSpace::PerspectiveCamera*>(&camera))
        {
            cameraNear = perspectiveCamera->GetNearPlane();
            cameraFar = perspectiveCamera->GetFarPlane();
        }

        std::ostringstream oss;
        oss << "[BlobShadow] receiverZ=" << (receiverZValid ? std::to_string(*receiverZ) : std::string("unset"))
            << " casterZ=" << casterZ
            << " resolvedZ=" << resolvedReceiverZ
            << " cameraNear=" << (cameraNear ? std::to_string(*cameraNear) : std::string("unset"))
            << " cameraFar=" << (cameraFar ? std::to_string(*cameraFar) : std::string("unset"))
            << "\n";
        DebugTrace(oss.str());
        --m_debugLogRemaining;
    }

    Transform shadowTransform{};
    shadowTransform.position = float3{
        renderTransform.position.x,
        receiverY + m_settings.receiverBias,
        resolvedReceiverZ
    };
    shadowTransform.rotation = float3{ DirectX::XM_PIDIV2, 0.0f, 0.0f };
    shadowTransform.scale = float3{
        width * scaleFactor,
        depth * scaleFactor,
        1.0f
    };

    DirectX::XMFLOAT4X4 wvp[3]{};
    DrawMatrix::CreateWorldMatrix(wvp[0], shadowTransform.position, shadowTransform.scale, shadowTransform.rotation);
    wvp[1] = camera.GetView();
    wvp[2] = camera.GetProjection();

    ShadowParams params{};
    params.color = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, alpha);
    params.softness = m_settings.softness;

    DrawQuad(wvp, params);
}

void BlobShadowRenderer::DrawQuad(DirectX::XMFLOAT4X4 wvp[3], const ShadowParams& params)
{
    if (!m_mesh || !m_vs || !m_ps)
    {
        return;
    }

    SetBlendMode(BLEND_ALPHA);
    SetCullingMode(D3D11_CULL_NONE);
    if (m_settings.debugNoDepthTest)
    {
        SetDepthTest(false);
    }
    else
    {
        SetDepthTestWrite(true, false);
    }

    m_vs->WriteBuffer(0, wvp);
    m_vs->Bind();
    ShadowParams writableParams = params;
    m_ps->WriteBuffer(0, &writableParams);
    m_ps->Bind();

    ID3D11DeviceContext* context = GetContext();
    UINT offset = 0;
    const Core::VertexBuffer& vertexBuffer = m_mesh->GetVertexBuffer();
    vertexBuffer.Bind(0, offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context->Draw(vertexBuffer.GetCount(), 0);

    SetDepthTest(true);
}
