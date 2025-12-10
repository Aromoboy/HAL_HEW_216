#include "Geometory.h"
#include <cmath>
#include <vector>
#include <DirectXMath.h>

MeshBuffer* Geometory::m_pBox;
MeshBuffer* Geometory::m_pCylinder;
MeshBuffer* Geometory::m_pSphere;
MeshBuffer* Geometory::m_pLines;
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
	SAFE_DELETE(m_pLines);
	SAFE_DELETE(m_pSphere);
	SAFE_DELETE(m_pCylinder);
	SAFE_DELETE(m_pBox);
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
	m_pLineShader[0]->WriteBuffer(0, m_WVP);
	m_pLineShader[0]->Bind();
	m_pLineShader[1]->Bind();
	m_pLines->Write(m_pLineVtx);
	m_pLines->Draw(m_lineCnt * 2);
	m_lineCnt = 0;
}

void Geometory::DrawBox()
{
	if (m_pBox == nullptr)
		return;
	m_pVS->WriteBuffer(0, m_WVP);
	m_pVS->Bind();
	m_pPS->Bind();
	m_pBox->Draw();
}
void Geometory::DrawCylinder()
{
	if (m_pCylinder == nullptr)
		return;
	m_pVS->WriteBuffer(0, m_WVP);
	m_pVS->Bind();
	m_pPS->Bind();
	m_pCylinder->Draw();
}
void Geometory::DrawSphere()
{
	if (m_pSphere == nullptr)
		return;
	m_pVS->WriteBuffer(0, m_WVP);
	m_pVS->Bind();
	m_pPS->Bind();
	m_pSphere->Draw();
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
        SAFE_DELETE(m_pBox);

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

        uint16_t indices[] = {
                0, 1, 2, 0, 2, 3,       // Front
                4, 5, 6, 4, 6, 7,       // Back
                8, 9, 10, 8, 10, 11,    // Left
                12, 13, 14, 12, 14, 15, // Right
                16, 17, 18, 16, 18, 19, // Top
                20, 21, 22, 20, 22, 23  // Bottom
        };

        MeshBuffer::Description desc = {};
        desc.pVtx = vertices;
        desc.vtxSize = sizeof(Vertex);
        desc.vtxCount = static_cast<UINT>(sizeof(vertices) / sizeof(vertices[0]));
        desc.isWrite = false;
        desc.pIdx = indices;
        desc.idxSize = sizeof(uint16_t);
        desc.idxCount = static_cast<UINT>(sizeof(indices) / sizeof(indices[0]));
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        m_pBox = new MeshBuffer();
        if (FAILED(m_pBox->Create(desc)))
        {
                SAFE_DELETE(m_pBox);
        }
}

void Geometory::MakeCylinder()
{
        SAFE_DELETE(m_pCylinder);

        const int segments = CIRCLE_DETAIL;
        const float radius = 0.5f;
        const float halfHeight = 0.5f;

        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;
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
                uint16_t i0 = static_cast<uint16_t>(i * 2);
                uint16_t i1 = static_cast<uint16_t>(i * 2 + 1);
                uint16_t i2 = static_cast<uint16_t>((i + 1) * 2);
                uint16_t i3 = static_cast<uint16_t>((i + 1) * 2 + 1);

                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i1);

                indices.push_back(i1);
                indices.push_back(i2);
                indices.push_back(i3);
        }

        // Top cap
        uint16_t topCenterIndex = static_cast<uint16_t>(vertices.size());
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
                uint16_t i0 = topCenterIndex;
                uint16_t i1 = static_cast<uint16_t>(topCenterIndex + i + 1);
                uint16_t i2 = static_cast<uint16_t>(topCenterIndex + i + 2);
                indices.push_back(i0);
                indices.push_back(i1);
                indices.push_back(i2);
        }

        // Bottom cap
        uint16_t bottomCenterIndex = static_cast<uint16_t>(vertices.size());
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
                uint16_t i0 = bottomCenterIndex;
                uint16_t i1 = static_cast<uint16_t>(bottomCenterIndex + i + 2);
                uint16_t i2 = static_cast<uint16_t>(bottomCenterIndex + i + 1);
                indices.push_back(i0);
                indices.push_back(i1);
                indices.push_back(i2);
        }

        MeshBuffer::Description desc = {};
        desc.pVtx = vertices.data();
        desc.vtxSize = sizeof(Vertex);
        desc.vtxCount = static_cast<UINT>(vertices.size());
        desc.isWrite = false;
        desc.pIdx = indices.data();
        desc.idxSize = sizeof(uint16_t);
        desc.idxCount = static_cast<UINT>(indices.size());
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        m_pCylinder = new MeshBuffer();
        if (FAILED(m_pCylinder->Create(desc)))
        {
                SAFE_DELETE(m_pCylinder);
        }
}

void Geometory::MakeSphere()
{
        SAFE_DELETE(m_pSphere);

        const int sliceCount = CIRCLE_DETAIL;
        const int stackCount = CIRCLE_DETAIL / 2;
        const float radius = 0.5f;

        std::vector<Vertex> vertices;
        std::vector<uint16_t> indices;
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
                        uint16_t i0 = static_cast<uint16_t>(stack * (sliceCount + 1) + slice);
                        uint16_t i1 = static_cast<uint16_t>(i0 + sliceCount + 1);
                        uint16_t i2 = static_cast<uint16_t>(i0 + 1);
                        uint16_t i3 = static_cast<uint16_t>(i1 + 1);

                        indices.push_back(i0);
                        indices.push_back(i1);
                        indices.push_back(i2);

                        indices.push_back(i2);
                        indices.push_back(i1);
                        indices.push_back(i3);
                }
        }

        MeshBuffer::Description desc = {};
        desc.pVtx = vertices.data();
        desc.vtxSize = sizeof(Vertex);
        desc.vtxCount = static_cast<UINT>(vertices.size());
        desc.isWrite = false;
        desc.pIdx = indices.data();
        desc.idxSize = sizeof(uint16_t);
        desc.idxCount = static_cast<UINT>(indices.size());
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        m_pSphere = new MeshBuffer();
        if (FAILED(m_pSphere->Create(desc)))
        {
                SAFE_DELETE(m_pSphere);
        }
}
void Geometory::MakeLine()
{
	m_pLineVtx = new LineVertex[MAX_LINE_NUM * 2];
	m_lineCnt = 0;
	MeshBuffer::Description desc = {};
	desc.pVtx = m_pLineVtx;
	desc.vtxCount = MAX_LINE_NUM * 2;
	desc.vtxSize = sizeof(LineVertex);
	desc.topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	desc.isWrite = true;
	m_pLines = new MeshBuffer();
	m_pLines->Create(desc);
}