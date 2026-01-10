#pragma once
#include <string>
#include "Vector.h"
#include "ICamera.h"
#include "TextureCache.h"
#include "MeshDX.h"
#include "Shader.h"
#include "TextureDX.h"
#include "Sprite.h"


class TitleRendererTexture : public Texture
{
public:
	TitleRendererTexture()
		: m_stringTextureScale(float2{ 1.0f, 1.0f })
	{

	}
	void DrawHud(const CameraSpace::ICamera& camera);
private:
	// 各文字列のテクスチャサイズ
	float2 m_stringTextureScale;

	
};

class TitleRenderer
{
public:


private:

	struct DrawSpriteData
	{
		float2 uvPos;
		float2 uvScale;
		float2 projOffset;
		float2 projScale;
		float r;
		float g;
		float b;
		float alpha;


		DrawSpriteData(float2 a_uvPos = { 0.0f, 0.0f }, float2 a_uvScale = { 1.0f, 1.0f }, float2 a_projOffset = { 0.0f, 0.0f }, float2 a_projScale = { 1.0f, 1.0f }, float3 a_rgb = { 1.0f,1.0f,1.0f }, float a_alpha = 1.0f)
			: uvPos(a_uvPos)
			, uvScale(a_uvScale)
			, projOffset(a_projOffset)
			, projScale(a_projScale)
			, r(a_rgb.x)
			, g(a_rgb.y)
			, b(a_rgb.z)
			, alpha(a_alpha)
		{}
	};


		// スプライト描画用関数
		void DrawSprite(std::shared_ptr<Texture2D> texture, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix, const DrawSpriteData& drawData);
		void DrawSprite(std::shared_ptr<Texture> texture, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix, const DrawSpriteData& drawData);
		void DrawSprite(std::shared_ptr<Texture2D> texture, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix);
		void MeshDraw();

		// 板として描画するためのメッシュ及びシェーダー、コンストラクタで自作する。
		std::shared_ptr<MeshDX> m_spriteMesh;
		std::shared_ptr<Shader> vs;
		std::shared_ptr<Shader> ps;

		// 描画用テクスチャ
		std::shared_ptr<Texture2D> m_cost;
		std::shared_ptr<Texture2D> m_iconFrame;
		std::shared_ptr<Texture2D> m_backFrame;

	

};

