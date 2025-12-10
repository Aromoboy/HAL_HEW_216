#include "Geometory.h"
#include <cmath>
#include <cstring>
#include <vector>
#include <DirectXMath.h>

std::shared_ptr<MeshDX> Geometory::m_pBox;
std::shared_ptr<MeshDX> Geometory::m_pCylinder;
std::shared_ptr<MeshDX> Geometory::m_pSphere;
Core::VertexBuffer Geometory::m_lineBuffer;
Shader* Geometory::m_pVS;
Shader* Geometory::m_pPS;
Shader* Geometory::m_pLineShader[2];
DirectX::XMFLOAT4X4 Geometory::m_WVP[3];
void* Geometory::m_pLineVtx;
int Geometory::m_lineCnt = 0;

void Geometory::Init()
{
	for (int i = 0; i < 3; ++i)
		DirectX::XMStoreFloat4x4(&m_WVP[i], DirectX::XMMatrixIdentity());
	MakeBox();
	MakeCylinder();
	MakeSphere();
	MakeVS();
	MakePS();
	MakeLineShader();
	MakeLine();
}
void Geometory::Uninit()
{
        SAFE_DELETE_ARRAY(m_pLineVtx);
        SAFE_DELETE(m_pLineShader[1]);
        SAFE_DELETE(m_pLineShader[0]);
        SAFE_DELETE(m_pPS);
        SAFE_DELETE(m_pVS);
        m_lineBuffer.Shutdown();
        m_pSphere.reset();
        m_pCylinder.reset();
        m_pBox.reset();
}

void Geometory::SetWorld(DirectX::XMFLOAT4X4 world)
{
	m_WVP[0] = world;
}
void Geometory::SetView(DirectX::XMFLOAT4X4 view)
{
	m_WVP[1] = view;
}
void Geometory::SetProjection(DirectX::XMFLOAT4X4 proj)
{
	m_WVP[2] = proj;
}

void Geometory::AddLine(DirectX::XMFLOAT3 start, DirectX::XMFLOAT3 end, DirectX::XMFLOAT4 color)
{
	if (m_lineCnt < MAX_LINE_NUM)
	{
		LineVertex* pVtx = reinterpret_cast<LineVertex*>(m_pLineVtx);
		pVtx[m_lineCnt * 2 + 0] = { start.x, start.y, start.z, color.x, color.y, color.z, color.w};
		pVtx[m_lineCnt * 2 + 1] = { end.x, end.y, end.z, color.x, color.y, color.z, color.w };
		++m_lineCnt;
	}
}
void Geometory::DrawLines()
{
        if (!m_lineBuffer.IsValid() || !m_pLineVtx)
        {
                return;
        }

        m_pLineShader[0]->WriteBuffer(0, m_WVP);
        m_pLineShader[0]->Bind();
        m_pLineShader[1]->Bind();

        if (m_lineBuffer.IsDynamic())
        {
                D3D11_MAPPED_SUBRESOURCE mapped{};
                if (SUCCEEDED(GetContext()->Map(m_lineBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                {
                        memcpy(mapped.pData, m_pLineVtx, sizeof(LineVertex) * m_lineCnt * 2);
                        GetContext()->Unmap(m_lineBuffer.Get(), 0);
                }
        }
        else
        {
                GetContext()->UpdateSubresource(m_lineBuffer.Get(), 0, nullptr, m_pLineVtx, 0, 0);
        }

        UINT offset = 0;
        m_lineBuffer.Bind(0, offset);
        GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        GetContext()->Draw(m_lineCnt * 2, 0);
        m_lineCnt = 0;
}

void Geometory::DrawBox()
{
        if (!m_pBox)
                return;
        m_pVS->WriteBuffer(0, m_WVP);
        m_pVS->Bind();
        m_pPS->Bind();
        UINT offset = 0;
        const Core::VertexBuffer& vtx = m_pBox->GetVertexBuffer();
        vtx.Bind(0, offset);
        const Core::IndexBuffer* idx = m_pBox->GetIndexBuffer();
        if (idx)
        {
                idx->Bind();
                GetContext()->IASetPrimitiveTopology(m_pBox->GetTopology());
                GetContext()->DrawIndexed(m_pBox->GetIndexCount(), 0, 0);
        }
        else
        {
                GetContext()->IASetPrimitiveTopology(m_pBox->GetTopology());
                GetContext()->Draw(vtx.GetCount(), 0);
        }
}
void Geometory::DrawCylinder()
{
        if (!m_pCylinder)
                return;
        m_pVS->WriteBuffer(0, m_WVP);
        m_pVS->Bind();
        m_pPS->Bind();
        UINT offset = 0;
        const Core::VertexBuffer& vtx = m_pCylinder->GetVertexBuffer();
        vtx.Bind(0, offset);
        const Core::IndexBuffer* idx = m_pCylinder->GetIndexBuffer();
        if (idx)
        {
                idx->Bind();
                GetContext()->IASetPrimitiveTopology(m_pCylinder->GetTopology());
                GetContext()->DrawIndexed(m_pCylinder->GetIndexCount(), 0, 0);
        }
        else
        {
                GetContext()->IASetPrimitiveTopology(m_pCylinder->GetTopology());
                GetContext()->Draw(vtx.GetCount(), 0);
        }
}
void Geometory::DrawSphere()
{
        if (!m_pSphere)
                return;
        m_pVS->WriteBuffer(0, m_WVP);
        m_pVS->Bind();
        m_pPS->Bind();
        UINT offset = 0;
        const Core::VertexBuffer& vtx = m_pSphere->GetVertexBuffer();
        vtx.Bind(0, offset);
        const Core::IndexBuffer* idx = m_pSphere->GetIndexBuffer();
        if (idx)
        {
                idx->Bind();
                GetContext()->IASetPrimitiveTopology(m_pSphere->GetTopology());
                GetContext()->DrawIndexed(m_pSphere->GetIndexCount(), 0, 0);
        }
        else
        {
                GetContext()->IASetPrimitiveTopology(m_pSphere->GetTopology());
                GetContext()->Draw(vtx.GetCount(), 0);
        }
}

void Geometory::MakeVS()
{
const char* VSCode = R"EOT(
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
	vout.pos = float4(vin.pos, 1.0f);
	vout.pos = mul(vout.pos, world);
	vout.pos = mul(vout.pos, view);
	vout.pos = mul(vout.pos, proj);
	vout.uv = vin.uv;
	return vout;
})EOT";

	m_pVS = new VertexShader();
	m_pVS->Compile(VSCode);
}

void Geometory::MakePS()
{
#ifdef _DEBUG
	const char* PSCode = R"EOT(
struct PS_IN {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};
float4 main(PS_IN pin) : SV_TARGET0 {
	float4 color = float4(1,1,1,1);
	float2 halfGrid = floor(abs(pin.uv) * 2.0f);
	float2 quatGrid = floor(abs(pin.uv) * 8.0f);

	float half = fmod(halfGrid.x + halfGrid.y, 2.0f);
	float quat = fmod(quatGrid.x + quatGrid.y, 2.0f);

	color.rgb = ((half * 0.1f) * quat + 0.45f) + (1 - quat) * 0.05f;
	return color;
})EOT";
#else
	const char* PSCode = R"EOT(
struct PS_IN {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};
float4 main(PS_IN pin) : SV_TARGET0 {
	return float4(1.0f, 0.0f, 1.0f, 1.0f);
})EOT";
#endif

	m_pPS = new PixelShader();
	m_pPS->Compile(PSCode);
}
void Geometory::MakeLineShader()
{
	const char* VSCode = R"EOT(
struct VS_IN {
	float3 pos : POSITION0;
	float4 color : COLOR0;
};
struct VS_OUT {
	float4 pos : SV_POSITION;
	float4 color : COLOR0;
};
cbuffer Matrix : register(b0) {
	float4x4 world;
	float4x4 view;
	float4x4 proj;
};
VS_OUT main(VS_IN vin) {
	VS_OUT vout;
	vout.pos = float4(vin.pos, 1.0f);
	vout.pos = mul(vout.pos, view);
	vout.pos = mul(vout.pos, proj);
	vout.color = vin.color;
	return vout;
})EOT";

	const char* PSCode = R"EOT(
struct PS_IN {
	float4 pos : SV_POSITION;
	float4 color : COLOR0;
};
float4 main(PS_IN pin) : SV_TARGET0 {
	return pin.color;
})EOT";

	m_pLineShader[0] = new VertexShader();
	m_pLineShader[0]->Compile(VSCode);
        m_pLineShader[1] = new PixelShader();
        m_pLineShader[1]->Compile(PSCode);
}

void Geometory::MakeBox()
{
        m_pBox.reset();

        const float half = 0.5f;

        Vertex vertices[] = {
                // Front
                {{-half, -half, +half}, {0.0f, 1.0f}},
                {{+half, -half, +half}, {1.0f, 1.0f}},
                {{+half, +half, +half}, {1.0f, 0.0f}},
                {{-half, +half, +half}, {0.0f, 0.0f}},
                // Back
                {{+half, -half, -half}, {0.0f, 1.0f}},
                {{-half, -half, -half}, {1.0f, 1.0f}},
                {{-half, +half, -half}, {1.0f, 0.0f}},
                {{+half, +half, -half}, {0.0f, 0.0f}},
                // Left
                {{-half, -half, -half}, {0.0f, 1.0f}},
                {{-half, -half, +half}, {1.0f, 1.0f}},
                {{-half, +half, +half}, {1.0f, 0.0f}},
                {{-half, +half, -half}, {0.0f, 0.0f}},
                // Right
                {{+half, -half, +half}, {0.0f, 1.0f}},
                {{+half, -half, -half}, {1.0f, 1.0f}},
                {{+half, +half, -half}, {1.0f, 0.0f}},
                {{+half, +half, +half}, {0.0f, 0.0f}},
                // Top
                {{-half, +half, +half}, {0.0f, 1.0f}},
                {{+half, +half, +half}, {1.0f, 1.0f}},
                {{+half, +half, -half}, {1.0f, 0.0f}},
                {{-half, +half, -half}, {0.0f, 0.0f}},
                // Bottom
                {{-half, -half, -half}, {0.0f, 1.0f}},
                {{+half, -half, -half}, {1.0f, 1.0f}},
                {{+half, -half, +half}, {1.0f, 0.0f}},
                {{-half, -half, +half}, {0.0f, 0.0f}},
        };

        uint32_t indices[] = {
                0, 1, 2, 0, 2, 3,       // Front
                4, 5, 6, 4, 6, 7,       // Back
                8, 9, 10, 8, 10, 11,    // Left
                12, 13, 14, 12, 14, 15, // Right
                16, 17, 18, 16, 18, 19, // Top
                20, 21, 22, 20, 22, 23  // Bottom
        };

        MeshDX::Description desc = {};
        desc.pVtx = vertices;
        desc.vtxSize = sizeof(Vertex);
        desc.vtxCount = static_cast<UINT>(sizeof(vertices) / sizeof(vertices[0]));
        desc.pIdx = indices;
        desc.idxSize = sizeof(uint32_t);
        desc.idxCount = static_cast<UINT>(sizeof(indices) / sizeof(indices[0]));
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        m_pBox = std::make_shared<MeshDX>(desc, 0);
}

void Geometory::MakeCylinder()
{
        m_pCylinder.reset();

        const int segments = CIRCLE_DETAIL;
        const float radius = 0.5f;
        const float halfHeight = 0.5f;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        vertices.reserve((segments + 1) * 2 + (segments + 1) * 2 + 2);

        // Side vertices
        for (int i = 0; i <= segments; ++i)
        {
                float t = static_cast<float>(i) / segments;
                float angle = DirectX::XM_2PI * t;
                float x = radius * cosf(angle);
                float z = radius * sinf(angle);

                vertices.push_back({{x, -halfHeight, z}, {t, 1.0f}});
                vertices.push_back({{x, +halfHeight, z}, {t, 0.0f}});
        }

        // Side indices
        for (int i = 0; i < segments; ++i)
        {
                uint32_t i0 = static_cast<uint32_t>(i * 2);
                uint32_t i1 = static_cast<uint32_t>(i * 2 + 1);
                uint32_t i2 = static_cast<uint32_t>((i + 1) * 2);
                uint32_t i3 = static_cast<uint32_t>((i + 1) * 2 + 1);

                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i1);

                indices.push_back(i1);
                indices.push_back(i2);
                indices.push_back(i3);
        }

        // Top cap
        uint32_t topCenterIndex = static_cast<uint32_t>(vertices.size());
        vertices.push_back({{0.0f, +halfHeight, 0.0f}, {0.5f, 0.5f}});
        for (int i = 0; i <= segments; ++i)
        {
                float t = static_cast<float>(i) / segments;
                float angle = DirectX::XM_2PI * t;
                float x = radius * cosf(angle);
                float z = radius * sinf(angle);
                vertices.push_back({{x, +halfHeight, z}, {x / radius * 0.5f + 0.5f, z / radius * 0.5f + 0.5f}});
        }

        for (int i = 0; i < segments; ++i)
        {
                uint32_t i0 = topCenterIndex;
                uint32_t i1 = static_cast<uint32_t>(topCenterIndex + i + 1);
                uint32_t i2 = static_cast<uint32_t>(topCenterIndex + i + 2);
                indices.push_back(i0);
                indices.push_back(i1);
                indices.push_back(i2);
        }

        // Bottom cap
        uint32_t bottomCenterIndex = static_cast<uint32_t>(vertices.size());
        vertices.push_back({{0.0f, -halfHeight, 0.0f}, {0.5f, 0.5f}});
        for (int i = 0; i <= segments; ++i)
        {
                float t = static_cast<float>(i) / segments;
                float angle = DirectX::XM_2PI * t;
                float x = radius * cosf(angle);
                float z = radius * sinf(angle);
                vertices.push_back({{x, -halfHeight, z}, {x / radius * 0.5f + 0.5f, z / radius * 0.5f + 0.5f}});
        }

        for (int i = 0; i < segments; ++i)
        {
                uint32_t i0 = bottomCenterIndex;
                uint32_t i1 = static_cast<uint32_t>(bottomCenterIndex + i + 2);
                uint32_t i2 = static_cast<uint32_t>(bottomCenterIndex + i + 1);
                indices.push_back(i0);
                indices.push_back(i1);
                indices.push_back(i2);
        }

        MeshDX::Description desc = {};
        desc.pVtx = vertices.data();
        desc.vtxSize = sizeof(Vertex);
        desc.vtxCount = static_cast<UINT>(vertices.size());
        desc.pIdx = indices.data();
        desc.idxSize = sizeof(uint32_t);
        desc.idxCount = static_cast<UINT>(indices.size());
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        m_pCylinder = std::make_shared<MeshDX>(desc, 0);
}

void Geometory::MakeSphere()
{
        m_pSphere.reset();

        const int sliceCount = CIRCLE_DETAIL;
        const int stackCount = CIRCLE_DETAIL / 2;
        const float radius = 0.5f;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        vertices.reserve((stackCount + 1) * (sliceCount + 1));

        for (int stack = 0; stack <= stackCount; ++stack)
        {
                float v = static_cast<float>(stack) / stackCount;
                float phi = DirectX::XM_PI * v;
                float y = radius * cosf(phi);
                float r = radius * sinf(phi);

                for (int slice = 0; slice <= sliceCount; ++slice)
                {
                        float u = static_cast<float>(slice) / sliceCount;
                        float theta = DirectX::XM_2PI * u;
                        float x = r * cosf(theta);
                        float z = r * sinf(theta);
                        vertices.push_back({{x, y, z}, {u, v}});
                }
        }

        for (int stack = 0; stack < stackCount; ++stack)
        {
                for (int slice = 0; slice < sliceCount; ++slice)
                {
                        uint32_t i0 = static_cast<uint32_t>(stack * (sliceCount + 1) + slice);
                        uint32_t i1 = static_cast<uint32_t>(i0 + sliceCount + 1);
                        uint32_t i2 = static_cast<uint32_t>(i0 + 1);
                        uint32_t i3 = static_cast<uint32_t>(i1 + 1);

                        indices.push_back(i0);
                        indices.push_back(i1);
                        indices.push_back(i2);

                        indices.push_back(i2);
                        indices.push_back(i1);
                        indices.push_back(i3);
                }
        }

        MeshDX::Description desc = {};
        desc.pVtx = vertices.data();
        desc.vtxSize = sizeof(Vertex);
        desc.vtxCount = static_cast<UINT>(vertices.size());
        desc.pIdx = indices.data();
        desc.idxSize = sizeof(uint32_t);
        desc.idxCount = static_cast<UINT>(indices.size());
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        m_pSphere = std::make_shared<MeshDX>(desc, 0);
}
void Geometory::MakeLine()
{
        m_pLineVtx = new LineVertex[MAX_LINE_NUM * 2];
        m_lineCnt = 0;
        m_lineBuffer.Init(m_pLineVtx, sizeof(LineVertex), MAX_LINE_NUM * 2, true);
}