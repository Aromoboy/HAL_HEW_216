// スプライトを描く処理をまとめている
// 画面表示や演出の描画から呼ばれる

#include "SpriteRenderer.h"


#include "ShaderList.h"
#include "DirectX.h"
#include "DrawMatrix.h"
#include "Defines.h"
//#include "DirectXMath.h"

SpriteRenderer::SpriteRenderer()
	: m_uvPos(0.0f, 0.0f)
	, m_uvScale(1.0f, 1.0f)
{
	const char* VS = R"EOT(
struct VS_IN {
	float3 pos : POSITION0;
	float2 uv : TEXCOORD0;
};
struct VS_OUT {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 color : COLOR0;
};
cbuffer Matrix : register(b0) {
	float4x4 world;
	float4x4 view;
	float4x4 proj;
};
cbuffer Param : register(b1) {
	float2 offset;
	float2 size;
	float2 uvPos;
	float2 uvScale;
	float4 color;
};
VS_OUT main(VS_IN vin) {
	VS_OUT vout;
	vout.pos = float4(vin.pos, 1.0f);
	vout.pos.xy *= size;
	vout.pos.xy += offset;
	vout.pos = mul(vout.pos, world);
	vout.pos = mul(vout.pos, view);
	vout.pos = mul(vout.pos, proj);
	vout.uv = vin.uv;
	vout.uv *= uvScale;
	vout.uv += uvPos;
	vout.color = color;
	return vout;
})EOT";
	const char* PS = R"EOT(
struct PS_IN {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 color : COLOR0;
};
Texture2D tex : register(t0);
SamplerState samp : register(s0);
float4 main(PS_IN pin) : SV_TARGET {
	return tex.Sample(samp, pin.uv) * pin.color;
})EOT";


	struct Vertex
	{
		float pos[3];
		float uv[2];
	} vtx[] = {
		{{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f}},
		{{ 0.5f, 0.5f, 0.0f}, {1.0f, 0.0f}},
		{{-0.5f,-0.5f, 0.0f}, {0.0f, 1.0f}},
		{{ 0.5f,-0.5f, 0.0f}, {1.0f, 1.0f}},
	};

	MeshDX::Description desc{};
	desc.pVtx = vtx;
	desc.vtxSize = sizeof(Vertex);
	desc.vtxCount = 4;
	desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

	m_spriteMesh = std::make_shared<MeshDX>(desc, -1);

	vs = std::make_shared<VertexShader>();
	vs->Compile(VS);
	
	ps = std::make_shared <PixelShader>();
	ps->Compile(PS);


}

void SpriteRenderer::SetUVPos(DirectX::XMFLOAT2 pos)
{
	m_uvPos = pos;
}

void SpriteRenderer::SetUVScale(DirectX::XMFLOAT2 scale)
{
	m_uvScale = scale;
}

SpriteRenderer::~SpriteRenderer()
{
}

void SpriteRenderer::Draw(Texture2D& texture, const Transform& transform, const CameraSpace::ICamera* camera)
{
	DirectX::XMFLOAT4X4 wvp[3];
	DirectX::XMFLOAT4 param[3];

	DrawMatrix::CreateWorldMatrix(wvp[0], transform.position, transform.scale, transform.rotation);

	wvp[1] = camera->GetView();
	wvp[2] = camera->GetProjection();

	param[0] = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
	param[1] = DirectX::XMFLOAT4(m_uvPos.x, m_uvPos.y, m_uvScale.x, m_uvScale.y);
	param[2] = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	ShaderList::SetWVP(wvp);

	vs->WriteBuffer(0, wvp);
	vs->WriteBuffer(1, param);
	vs->Bind();


	ps->SetTexture(0, texture.Get());
	ps->Bind();

	MeshDraw();


}



void SpriteRenderer::MeshDraw()
{
	ID3D11DeviceContext* pContext = GetContext();
	UINT offset = 0;

	const Core::VertexBuffer& vertexBuffer = m_spriteMesh->GetVertexBuffer();
	vertexBuffer.Bind(0, offset);


	if (m_spriteMesh->GetIndexCount() > 0 && m_spriteMesh->GetIndexBuffer()->IsValid())
	{
		m_spriteMesh->GetIndexBuffer()->Bind();
		GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		GetContext()->DrawIndexed(m_spriteMesh->GetIndexCount(), 0, 0);
	}
	else
	{
		// 頂点バッファのみで描画
		GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		pContext->Draw(vertexBuffer.GetCount(), 0);

	}

}