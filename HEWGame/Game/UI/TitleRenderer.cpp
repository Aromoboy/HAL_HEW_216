#include "TitleRenderer.h"
#include "DirectX.h"
#include "DirectXMath.h"
#include "ShaderList.h"
#include "DrawMatrix.h"
#include "Sprite.h"

#include <sstream>
#include <iomanip>
#include <array>
#include <algorithm>


TitleRenderer::TitleRenderer()
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

void TitleRenderer::Draw(std::shared_ptr<Texture2D> a_texture, float2 a_pos, float2 a_size)
{
	float3 pos(0.0f, 0.0f, 1.0f);
	float3 size(1.0f, 1.0f, 1.0f);


	DirectX::XMFLOAT4X4 world;
	DirectX::XMFLOAT4X4 view;
	DirectX::XMFLOAT4X4 proj;
	
	DrawMatrix::CreateWorldMatrix(world, pos, size, float3(0.01f, 0.01f, 0.01f), false);

	DrawMatrix::CreateViewMatrix(view, float3(0.0f, 0.0f, -10.0f), float3(0.0f, 0.0f, 1000.0f), float3(0.0f, 1.0f, 0.0f));
	DrawMatrix::CreateProjectionMatrix(proj, 60.0f, 16.0f / 9.0f, 0.0001f, 1000.0f);
	DrawSprite(a_texture, world, view, proj, DrawSpriteData(float2(0.0f, 0.0f), float2(1.0f, 1.0f), a_pos, a_size));
}

void TitleRenderer::DrawSprite(std::shared_ptr<Texture2D> texture, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix, const DrawSpriteData& drawData)
{
	// 各マトリクスと描画用のパラメーター
	DirectX::XMFLOAT4X4 wvp[3];
	DirectX::XMFLOAT4 param[3];

	// 各マトリクスを設定
	wvp[0] = worldMatrix;
	wvp[1] = viewMatrix;
	wvp[2] = projMatrix;


	// ステータスを設定
	param[0] = DirectX::XMFLOAT4(drawData.projOffset.x, drawData.projOffset.y, drawData.projScale.x, drawData.projScale.y);
	param[1] = DirectX::XMFLOAT4(drawData.uvPos.x, drawData.uvPos.y, drawData.uvScale.x, drawData.uvScale.y);
	param[2] = DirectX::XMFLOAT4(drawData.r, drawData.g, drawData.b, drawData.alpha);


	// シェーダーをバインド
	ShaderList::SetWVP(wvp);

	vs->WriteBuffer(0, wvp);
	vs->WriteBuffer(1, param);
	vs->Bind();

	ps->SetTexture(0, texture->Get());
	ps->Bind();

	// 描画
	MeshDraw();

}


void TitleRenderer::MeshDraw()
{
	// �R���e�L�X�g���擾
	ID3D11DeviceContext* pContext = GetContext();
	UINT offset = 0;

	// ���_���o�C���h
	const Core::VertexBuffer& vertexBuffer = m_spriteMesh->GetVertexBuffer();
	vertexBuffer.Bind(0, offset);

	// ���_�o�b�t�@�݂̂ŕ`��
	GetContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	pContext->Draw(vertexBuffer.GetCount(), 0);

}
