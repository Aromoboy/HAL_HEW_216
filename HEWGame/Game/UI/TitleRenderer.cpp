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

void TitleRenderer::DrawSprite(std::shared_ptr<Texture2D> texture, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix)
{
	// 各マトリクスと描画用のパラメーター
	DirectX::XMFLOAT4X4 wvp[3];
	DirectX::XMFLOAT4 param[3];

	// 各マトリクスを設定
	wvp[0] = worldMatrix;
	wvp[1] = viewMatrix;
	wvp[2] = projMatrix;


	// ステータスを設定
	param[0] = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
	param[1] = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
	param[2] = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);


	// シェーダーをバインド
	ShaderList::SetWVP(wvp);

	vs->WriteBuffer(0, wvp);
	vs->WriteBuffer(1, param);
	vs->Bind();

	ps->SetTexture(0, texture->Get());
	ps->Bind();

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

//void TitleRendererTexture::DrawHud(const CameraSpace::ICamera& camera)
//{
//	const DirectX::XMFLOAT4X4& view = camera.GetView();
//	const DirectX::XMFLOAT4X4& proj = camera.GetProjection();
//
//	DirectX::XMFLOAT4X4 world;
//	float3 position;
//	float3 scale;
//	float2 offset;
//
//
//	// バックフレーム描画
//	offset.x = kHudCostIconPositionOffset.x;
//	offset.y = kHudCostIconPositionOffset.y;
//	position.x = offset.x;
//	position.y = offset.y;
//	position.z = 1.0f;
//	scale.x = kHudCostIconScaleOffset;
//	scale.y = kHudCostIconScaleOffset;
//	scale.z = 1.0f;
//
//	DrawMatrix::CreateWorldMatrix(world, position, scale, float3{ 0.0f,0.0f,0.0f });
//
//	DrawSprite(m_cost, world, view, proj);
//}
