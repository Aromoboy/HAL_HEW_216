// タイトル画面の表示を描く役目を持つ
// タイトルシーンの描画から呼ばれる

#pragma once
#include <string>
#include "Vector.h"
#include "ICamera.h"
#include "TextureCache.h"
#include "MeshDX.h"
#include "Shader.h"
#include "TextureDX.h"



class TitleRendererTexture : public Texture
{
public:
	TitleRendererTexture()
		: m_stringTextureScale(float2{ 1.0f, 1.0f })
	{

	}
	void DrawHud(const CameraSpace::ICamera& camera);
private:
	// ẽeNX`TCY
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


		// XvCg`p֐
		void DrawSprite(std::shared_ptr<Texture2D> texture, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix, const DrawSpriteData& drawData);
		void DrawSprite(std::shared_ptr<Texture> texture, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix, const DrawSpriteData& drawData);
		void DrawSprite(std::shared_ptr<Texture2D> texture, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix);
		void MeshDraw();

		// Ƃĕ`悷邽߂̃bVyуVF[_[ARXgN^Ŏ삷B
		std::shared_ptr<MeshDX> m_spriteMesh;
		std::shared_ptr<Shader> vs;
		std::shared_ptr<Shader> ps;

		// `peNX`
		std::shared_ptr<Texture2D> m_cost;
		std::shared_ptr<Texture2D> m_iconFrame;
		std::shared_ptr<Texture2D> m_backFrame;

	

};

