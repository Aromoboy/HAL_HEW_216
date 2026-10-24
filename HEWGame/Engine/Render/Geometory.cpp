#include "Geometory.h"

#include <cmath>
#include <cstdint>
#include <vector>

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
	delete[] static_cast<LineVertex*>(m_pLineVtx);
	m_pLineVtx = nullptr;
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
	// ミカ：線がないフレームはバッファ転送と描画状態の変更を行わない
	if (m_lineCnt == 0) return;
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

void Geometory::MakeBox()
{
        const int MAX_VERTEX = 4 * 6;
        const int MAX_INDEX = 6 * 6;

        Vertex vtx[MAX_VERTEX] =
        {
                // -Z
                {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f}},
                {{ 0.5f, 0.5f, -0.5f}, {1.0f, 0.0f}},
                {{-0.5f,-0.5f, -0.5f}, {0.0f, 1.0f}},
                {{ 0.5f,-0.5f, -0.5f}, {1.0f, 1.0f}},

                // Z
                {{ 0.5f, 0.5f,  0.5f}, {0.0f, 0.0f}},
                {{-0.5f, 0.5f,  0.5f}, {1.0f, 0.0f}},
                {{ 0.5f,-0.5f,  0.5f}, {0.0f, 1.0f}},
                {{-0.5f,-0.5f,  0.5f}, {1.0f, 1.0f}},

                // -X
                {{-0.5f, 0.5f,  0.5f}, {0.0f, 0.0f}},
                {{-0.5f, 0.5f, -0.5f}, {1.0f, 0.0f}},
                {{-0.5f,-0.5f,  0.5f}, {0.0f, 1.0f}},
                {{-0.5f,-0.5f, -0.5f}, {1.0f, 1.0f}},

                // X
                {{ 0.5f, 0.5f,  -0.5f},{0.0f, 0.0f}},
                {{ 0.5f, 0.5f,  0.5f}, {1.0f, 0.0f}},
                {{ 0.5f,-0.5f,  -0.5f},{0.0f, 1.0f}},
                {{ 0.5f,-0.5f,  0.5f}, {1.0f, 1.0f}},

                // -Y
                {{  0.5f, -0.5f, -0.5f},{0.0f, 0.0f}},
                {{  0.5f, -0.5f,  0.5f},{1.0f, 0.0f}},
                {{ -0.5f, -0.5f, -0.5f},{0.0f, 1.0f}},
                {{ -0.5f, -0.5f,  0.5f},{1.0f, 1.0f}},

                // Y
                {{ -0.5f, 0.5f, -0.5f},{0.0f, 0.0f}},
                {{ -0.5f, 0.5f,  0.5f},{1.0f, 0.0f}},
                {{  0.5f, 0.5f, -0.5f},{0.0f, 1.0f}},
                {{  0.5f, 0.5f,  0.5f},{1.0f, 1.0f}},

        };

        int idx[MAX_INDEX] =
        {
                0,1,2, 1,3,2, // -Z
                4,5,6, 5,7,6,  //  Z
                8,9,10, 9,11,10,
                12,13,14, 13,15,14,
                16,17,18, 17,19,18,
                20,21,22, 21,23,22,
        };

        MeshBuffer::Description desc = {};
        desc.pVtx = vtx;
        desc.vtxCount = MAX_VERTEX;
        desc.vtxSize = sizeof(vtx[0]);
        desc.pIdx = idx;
        desc.idxCount = MAX_INDEX;
        desc.idxSize = sizeof(idx[0]);
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;


        m_pBox = new MeshBuffer();
        m_pBox->Create(desc);
}

void Geometory::MakeCylinder()
{
        const float radius = 0.5f;
        const float halfHeight = 0.5f;
        const int slices = CIRCLE_DETAIL;
        const float step = DirectX::XM_2PI / static_cast<float>(slices);

        std::vector<Vertex> vertices;
        vertices.reserve((slices + 1) * 2 + (slices + 1) * 2 + 2);

        std::vector<uint16_t> indices;
        indices.reserve(slices * 6 * 2);

        // 側面頂点
        for (int i = 0; i <= slices; ++i)
        {
                float angle = step * static_cast<float>(i);
                float x = std::cos(angle) * radius;
                float z = std::sin(angle) * radius;
                float u = static_cast<float>(i) / static_cast<float>(slices);

                vertices.push_back({ { x,  halfHeight, z }, { u, 0.0f } });
                vertices.push_back({ { x, -halfHeight, z }, { u, 1.0f } });
        }

        // 側面インデックス
        for (int i = 0; i < slices; ++i)
        {
                uint16_t top0 = static_cast<uint16_t>(i * 2);
                uint16_t bottom0 = top0 + 1;
                uint16_t top1 = static_cast<uint16_t>(i * 2 + 2);
                uint16_t bottom1 = top1 + 1;

                indices.push_back(top0);
                indices.push_back(bottom0);
                indices.push_back(top1);

                indices.push_back(top1);
                indices.push_back(bottom0);
                indices.push_back(bottom1);
        }

        // 上面中心
        uint16_t topCenter = static_cast<uint16_t>(vertices.size());
        vertices.push_back({ { 0.0f, halfHeight, 0.0f }, { 0.5f, 0.5f } });

        // 上面リング
        for (int i = 0; i <= slices; ++i)
        {
                float angle = step * static_cast<float>(i);
                float x = std::cos(angle) * radius;
                float z = std::sin(angle) * radius;
                float u = 0.5f + 0.5f * std::cos(angle);
                float v = 0.5f - 0.5f * std::sin(angle);
                vertices.push_back({ { x, halfHeight, z }, { u, v } });
        }

        for (int i = 0; i < slices; ++i)
        {
                uint16_t ring0 = topCenter + 1 + static_cast<uint16_t>(i);
                uint16_t ring1 = ring0 + 1;
                indices.push_back(topCenter);
                indices.push_back(ring0);
                indices.push_back(ring1);
        }

        // 下面中心
        uint16_t bottomCenter = static_cast<uint16_t>(vertices.size());
        vertices.push_back({ { 0.0f, -halfHeight, 0.0f }, { 0.5f, 0.5f } });

        // 下面リング
        for (int i = 0; i <= slices; ++i)
        {
                float angle = step * static_cast<float>(i);
                float x = std::cos(angle) * radius;
                float z = std::sin(angle) * radius;
                float u = 0.5f + 0.5f * std::cos(angle);
                float v = 0.5f + 0.5f * std::sin(angle);
                vertices.push_back({ { x, -halfHeight, z }, { u, v } });
        }

        for (int i = 0; i < slices; ++i)
        {
                uint16_t ring0 = bottomCenter + 1 + static_cast<uint16_t>(i);
                uint16_t ring1 = ring0 + 1;
                indices.push_back(bottomCenter);
                indices.push_back(ring1);
                indices.push_back(ring0);
        }

        MeshBuffer::Description desc = {};
        desc.pVtx = vertices.data();
        desc.vtxCount = static_cast<UINT>(vertices.size());
        desc.vtxSize = sizeof(Vertex);
        desc.pIdx = indices.data();
        desc.idxCount = static_cast<UINT>(indices.size());
        desc.idxSize = sizeof(uint16_t);
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        m_pCylinder = new MeshBuffer();
        m_pCylinder->Create(desc);
}

void Geometory::MakeSphere()
{
        const float radius = 0.5f;
        const int slices = CIRCLE_DETAIL;
        const int stacks = CIRCLE_DETAIL;

        std::vector<Vertex> vertices;
        vertices.reserve((slices + 1) * (stacks + 1));

        std::vector<uint16_t> indices;
        indices.reserve(slices * stacks * 6);

        for (int stack = 0; stack <= stacks; ++stack)
        {
                float v = static_cast<float>(stack) / static_cast<float>(stacks);
                float phi = DirectX::XM_PI * v;
                float sinPhi = std::sin(phi);
                float cosPhi = std::cos(phi);

                for (int slice = 0; slice <= slices; ++slice)
                {
                        float u = static_cast<float>(slice) / static_cast<float>(slices);
                        float theta = DirectX::XM_2PI * u;
                        float sinTheta = std::sin(theta);
                        float cosTheta = std::cos(theta);

                        float x = radius * sinPhi * cosTheta;
                        float y = radius * cosPhi;
                        float z = radius * sinPhi * sinTheta;
                        vertices.push_back({ { x, y, z }, { u, v } });
                }
        }

        int ringStride = slices + 1;
        for (int stack = 0; stack < stacks; ++stack)
        {
                for (int slice = 0; slice < slices; ++slice)
                {
                        uint16_t idx0 = static_cast<uint16_t>(stack * ringStride + slice);
                        uint16_t idx1 = static_cast<uint16_t>(idx0 + 1);
                        uint16_t idx2 = static_cast<uint16_t>((stack + 1) * ringStride + slice);
                        uint16_t idx3 = static_cast<uint16_t>(idx2 + 1);

                        indices.push_back(idx0);
                        indices.push_back(idx2);
                        indices.push_back(idx1);

                        indices.push_back(idx1);
                        indices.push_back(idx2);
                        indices.push_back(idx3);
                }
        }

        MeshBuffer::Description desc = {};
        desc.pVtx = vertices.data();
        desc.vtxCount = static_cast<UINT>(vertices.size());
        desc.vtxSize = sizeof(Vertex);
        desc.pIdx = indices.data();
        desc.idxCount = static_cast<UINT>(indices.size());
        desc.idxSize = sizeof(uint16_t);
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        m_pSphere = new MeshBuffer();
        m_pSphere->Create(desc);
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
