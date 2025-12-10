#include "DebugColliderRenderer.h"

#include <cmath>
#include <cstdint>
#include <vector>
#include <DirectXMath.h>
#include "Core/IndexBuffer.h"
#include "ICamera.h"
#include "Core/VertexBuffer.h"
#include "DirectX.h"
#include "Shader.h"

namespace
{
    struct LineVertex
    {
        DirectX::XMFLOAT3 pos;
        DirectX::XMFLOAT4 color;
    };

    class LineBatch
    {
    public:
        void Clear()
        {
            m_vertices.clear();
            m_indices.clear();
            m_vertexBuffer.Shutdown();
            m_indexBuffer.Shutdown();
        }

        void AddLine(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const DirectX::XMFLOAT4& color)
        {
            const uint32_t baseIndex = static_cast<uint32_t>(m_vertices.size());
            m_vertices.push_back(LineVertex{ start, color });
            m_vertices.push_back(LineVertex{ end, color });
            m_indices.push_back(baseIndex);
            m_indices.push_back(baseIndex + 1);
        }

        bool Upload()
        {
            m_vertexBuffer.Shutdown();
            m_indexBuffer.Shutdown();

            if (m_vertices.empty())
            {
                return false;
            }

            HRESULT hr = m_vertexBuffer.Init(m_vertices.data(), sizeof(LineVertex), static_cast<UINT>(m_vertices.size()));
            if (FAILED(hr))
            {
                return false;
            }

            hr = m_indexBuffer.Init(m_indices.data(), sizeof(uint32_t), static_cast<UINT>(m_indices.size()));
            if (FAILED(hr))
            {
                m_vertexBuffer.Shutdown();
                m_indexBuffer.Shutdown();
                return false;
            }

            return m_vertexBuffer.IsValid() && m_indexBuffer.IsValid();
        }

        void Bind() const
        {
            m_vertexBuffer.Bind(0, m_vertexBuffer.GetStride(), 0);
            m_indexBuffer.Bind();
        }

        UINT IndexCount() const
        {
            if (m_indexBuffer.IsValid())
            {
                return m_indexBuffer.GetCount();
            }

            return static_cast<UINT>(m_indices.size());
        }

    private:
        std::vector<LineVertex> m_vertices;
        std::vector<uint32_t> m_indices;
        Core::VertexBuffer m_vertexBuffer;
        Core::IndexBuffer m_indexBuffer;
    };

    LineBatch g_lineBatch;
    std::shared_ptr<VertexShader> g_lineVS;
    std::shared_ptr<PixelShader> g_linePS;

    DirectX::XMFLOAT4 ToFloat4(const Color& c)
    {
        return DirectX::XMFLOAT4{ c.r, c.g, c.b, c.a };
    }

    DirectX::XMFLOAT3 ToFloat3(const float2& point, float z)
    {
        return DirectX::XMFLOAT3{ point.x, point.y, z };
    }

    void EnsureLineShaders()
    {
        if (g_lineVS && g_linePS)
        {
            return;
        }

        const char* kVS = R"EOT(
struct VS_IN {
        float3 pos : POSITION;
        float4 color : COLOR0;
};
struct VS_OUT {
        float4 pos : SV_POSITION;
        float4 color : COLOR0;
};
cbuffer VSConstants : register(b0) {
        float4x4 gWorldViewProj;
};
VS_OUT main(VS_IN vin) {
        VS_OUT vout;
        vout.pos = mul(float4(vin.pos, 1.0f), gWorldViewProj);
        vout.color = vin.color;
        return vout;
})EOT";

        const char* kPS = R"EOT(
struct PS_IN {
        float4 pos : SV_POSITION;
        float4 color : COLOR0;
};
float4 main(PS_IN pin) : SV_TARGET
{
        return pin.color;
})EOT";

        g_lineVS = std::make_shared<VertexShader>();
        g_lineVS->Compile(kVS);
        g_linePS = std::make_shared<PixelShader>();
        g_linePS->Compile(kPS);
    }

    void PushLine(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const DirectX::XMFLOAT4& color)
    {
        g_lineBatch.AddLine(start, end, color);
    }
}

namespace DebugColliderRenderer
{
    void DrawAABB(const AABB& bounds, Color c)
    {
        const float z = (bounds.min.z + bounds.max.z) * 0.5f;
        const DirectX::XMFLOAT3 leftUp{ bounds.min.x, bounds.max.y, z };
        const DirectX::XMFLOAT3 rightUp{ bounds.max.x, bounds.max.y, z };
        const DirectX::XMFLOAT3 leftDown{ bounds.min.x, bounds.min.y, z };
        const DirectX::XMFLOAT3 rightDown{ bounds.max.x, bounds.min.y, z };
        const DirectX::XMFLOAT4 color = ToFloat4(c);

        PushLine(leftUp, leftDown, color);
        PushLine(leftUp, rightUp, color);
        PushLine(rightDown, leftDown, color);
        PushLine(rightDown, rightUp, color);
    }

    void DrawHull(const std::vector<float2>& pts, float z, Color c)
    {
        if (pts.size() < 2)
        {
            return;
        }

        const DirectX::XMFLOAT4 color = ToFloat4(c);
        for (size_t i = 0; i < pts.size(); ++i)
        {
            const size_t next = (i + 1) % pts.size();
            PushLine(ToFloat3(pts[i], z), ToFloat3(pts[next], z), color);
        }
    }

    void DrawCircle(float2 center, float radius, float z, Color c)
    {
        constexpr int kSegments = 32;
        const float step = DirectX::XM_2PI / static_cast<float>(kSegments);
        const DirectX::XMFLOAT4 color = ToFloat4(c);

        for (int i = 0; i < kSegments; ++i)
        {
            const float startAngle = step * static_cast<float>(i);
            const float endAngle = step * static_cast<float>(i + 1);
            const DirectX::XMFLOAT3 start{
                center.x + std::cos(startAngle) * radius,
                center.y + std::sin(startAngle) * radius,
                z };
            const DirectX::XMFLOAT3 end{
                center.x + std::cos(endAngle) * radius,
                center.y + std::sin(endAngle) * radius,
                z };
            PushLine(start, end, color);
        }
    }

    void DrawCapsule(float2 p0, float2 p1, float radius, float z, Color c)
    {
        const float dx = p1.x - p0.x;
        const float dy = p1.y - p0.y;
        const float length = std::sqrt(dx * dx + dy * dy);

        float2 axis{ 0.0f, 1.0f };
        if (length > 1e-4f)
        {
            const float invLen = 1.0f / length;
            axis.x = dx * invLen;
            axis.y = dy * invLen;
        }
        const float2 normal{ -axis.y, axis.x };
        const float2 offset{ normal.x * radius, normal.y * radius };
        const DirectX::XMFLOAT4 color = ToFloat4(c);

        const float2 a0{ p0.x + offset.x, p0.y + offset.y };
        const float2 a1{ p1.x + offset.x, p1.y + offset.y };
        const float2 b0{ p0.x - offset.x, p0.y - offset.y };
        const float2 b1{ p1.x - offset.x, p1.y - offset.y };

        PushLine(ToFloat3(a0, z), ToFloat3(a1, z), color);
        PushLine(ToFloat3(b0, z), ToFloat3(b1, z), color);

        const float startAngle = std::atan2(axis.y, axis.x);
        const float step = DirectX::XM_PI / 16.0f;

        for (int i = 0; i <= 16; ++i)
        {
            const float angle0 = startAngle - DirectX::XM_PIDIV2 + step * static_cast<float>(i);
            const float angle1 = startAngle - DirectX::XM_PIDIV2 + step * static_cast<float>(i + 1);
            const DirectX::XMFLOAT3 arcStart{
                p1.x + std::cos(angle0) * radius,
                p1.y + std::sin(angle0) * radius,
                z };
            const DirectX::XMFLOAT3 arcEnd{
                p1.x + std::cos(angle1) * radius,
                p1.y + std::sin(angle1) * radius,
                z };
            PushLine(arcStart, arcEnd, color);
        }

        for (int i = 0; i <= 16; ++i)
        {
            const float angle0 = startAngle + DirectX::XM_PIDIV2 + step * static_cast<float>(i);
            const float angle1 = startAngle + DirectX::XM_PIDIV2 + step * static_cast<float>(i + 1);
            const DirectX::XMFLOAT3 arcStart{
                p0.x + std::cos(angle0) * radius,
                p0.y + std::sin(angle0) * radius,
                z };
            const DirectX::XMFLOAT3 arcEnd{
                p0.x + std::cos(angle1) * radius,
                p0.y + std::sin(angle1) * radius,
                z };
            PushLine(arcStart, arcEnd, color);
        }
    }

    void Flush(const CameraSpace::ICamera& camera)
    {
        if (g_lineBatch.IndexCount() == 0)
        {
            return;
        }

        EnsureLineShaders();

        if (!g_lineVS || !g_linePS)
        {
            g_lineBatch.Clear();
            return;
        }

        if (!g_lineBatch.Upload())
        {
            g_lineBatch.Clear();
            return;
        }

        struct VSConstants
        {
            DirectX::XMFLOAT4X4 worldViewProj;
        };

        VSConstants constants{};
        DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&camera.GetView());
        DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&camera.GetProjection());
        DirectX::XMStoreFloat4x4(&constants.worldViewProj, DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(world, view), proj));

        g_lineVS->WriteBuffer(0, &constants);
        g_lineVS->Bind();
        g_linePS->Bind();

        g_lineBatch.Bind();
        GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        GetContext()->DrawIndexed(g_lineBatch.IndexCount(), 0, 0);

        g_lineBatch.Clear();
    }
}
