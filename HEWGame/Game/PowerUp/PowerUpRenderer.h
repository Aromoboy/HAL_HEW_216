#pragma once

#include <string>
#include "Vector.h"
#include "ICamera.h"
#include "TextureCache.h"
#include "MeshDX.h"
#include "Shader.h"
#include "TextureDX.h"



struct PowerUpHudLayoutConfig
{
	uint32_t textureWidth = 512;
	uint32_t textureHeight = 256;
	float marginPixels = 24.0f;
	float innerPaddingPixels = 12.0f;
	float fontSizePixels = 28.0f;
	float warningThresholdSeconds = 5.0f;
	float shadowOffsetX = 1.5f;
	float shadowOffsetY = 2.0f;
	DirectX::XMFLOAT4 normalTextColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT4 warningTextColor{ 1.0f, 0.6f, 0.2f, 1.0f };
};




// 競合を避けるためこちらに作成、後で合併するかフォントアトラスを実装したい

enum PowerUpTextureType
{
	NodeName,
	NodeInfomation,
	NodeLevel,
	NodeCost,
	MaxNodeType,

};

struct TextBounds
{
	int top;
	int bottom;
	int height;

	TextBounds()
		: top(0)
		, bottom(0)
		, height(0)
	{
	}
};

class PowerUpStringTexture : public Texture
{
public:
	PowerUpStringTexture()
		: m_stringTextureScale(float2{ 1.0f, 1.0f })
	{
		
	}



	void UpdatePixels(const void* data, UINT pitch)
	{
		if (!GetContext())
		{
			return;
		}
		if (!m_pTex)
		{
			return;
		}
		GetContext()->UpdateSubresource(m_pTex, 0, nullptr, data, pitch, 0);
	}

	void SetScale(float2 a_scale)
	{
		m_stringTextureScale = a_scale;
	}

	float2 GetScale()
	{
		return m_stringTextureScale;
	}

	TextBounds& GetBound()
	{
		return bound;
	}

private:
	// 各文字列のテクスチャサイズ
	float2 m_stringTextureScale;

	// テクスチャの表示範囲
	TextBounds bound;
};


class PowerUpRenderer
{
public:
	
	

	struct NodeRenderData
	{
		std::shared_ptr<Texture2D> texture;
		std::shared_ptr<PowerUpStringTexture> stringTexture;
		float2 drawPosition;
		float2 drawScale;

		
		// ノードに描画する文字列及び数値
		bool dirty;
		std::string powerUpName;
		std::string powerUpInfomation;
		float level;
		float maxLevel;
		float cost;

		NodeRenderData(
		std::shared_ptr<Texture2D> a_texture,
		std::shared_ptr<PowerUpStringTexture> a_stringTexture,
		float2 a_drawPosition,
		float2 a_drawScale,
		bool a_dirty,
		std::string a_powerUpName,
		std::string a_powerUpInfomation,
		float a_level,
		float a_maxLevel,
		float a_cost)
			: texture(a_texture)
			, stringTexture(a_stringTexture)
			, drawPosition(a_drawPosition)
			, drawScale(a_drawScale)
			, dirty(a_dirty)
			, powerUpName(a_powerUpName)
			, powerUpInfomation(a_powerUpInfomation)
			, level(a_level)
			, maxLevel(a_maxLevel)
			, cost(a_cost)
		{
		}
	};



	PowerUpRenderer(TextureCache& cache);
	~PowerUpRenderer();

	bool Initialize(const PowerUpHudLayoutConfig& config);
	void Shutdown();

	void DrawCost(int cost, const CameraSpace::ICamera& camera);
	void DrawHud(const CameraSpace::ICamera& camera);
	void DrawCursor(std::shared_ptr<Texture2D> texture, float2 a_position, float2 a_scale, const CameraSpace::ICamera&, float a_alpha);
	void DrawNode(const NodeRenderData& data , const CameraSpace::ICamera&);
	void DrawBackground(const CameraSpace::ICamera& camera, float uvProgress);



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


		DrawSpriteData(float2 a_uvPos = {0.0f, 0.0f}, float2 a_uvScale = { 1.0f, 1.0f }, float2 a_projOffset = { 0.0f, 0.0f }, float2 a_projScale = { 1.0f, 1.0f }, float3 a_rgb = {1.0f,1.0f,1.0f}, float a_alpha = 1.0f)
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

	struct UVRegion
	{
		float startY;
		float height;
	};

	void DrawSprite(std::shared_ptr<Texture2D> texture, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix, const DrawSpriteData& drawData);
	void DrawSprite(std::shared_ptr<Texture2D> texture, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix);
	void DrawSprite(std::shared_ptr<Texture> texture, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix, const DrawSpriteData& drawData);

	void MeshDraw();

	// 板として描画するためのメッシュ及びシェーダー、コンストラクタで自作する 
	std::shared_ptr<MeshDX> m_spriteMesh;
	std::shared_ptr<Shader> vs;
	std::shared_ptr<Shader> ps;

	// 描画用テクスチャ
	std::shared_ptr<Texture2D> m_number;
	std::shared_ptr<Texture2D> m_cost;
	std::shared_ptr<Texture2D> m_iconFrame;
	std::shared_ptr<Texture2D> m_backFrame;
	std::shared_ptr<Texture2D> m_background;
	std::shared_ptr<Texture2D> m_fairy;

	// 文字描画用関数
	bool CreateTextResources();
	bool CreateGuidelineTexture();
	void DestroyTextResources();
	void UpdateBitmap(const NodeRenderData& data);
	void UploadTexture(const NodeRenderData& data);
	void RenderString(const NodeRenderData& data, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix, int uvNum, float size);
	void DetectTextBounds(TextBounds& bounds, const uint8_t* rgba, int width, int height);
	UVRegion ComputeUV(const TextBounds& bounds, int textureHeight);
	const std::wstring UTF8toWString(const std::string srcUTF8) const;

	bool TryGetViewportSize(float& width, float& height) const;
	std::wstring BuildHudString(const NodeRenderData& data) const;


	// 各リソース
	PowerUpHudLayoutConfig m_config{};
	std::shared_ptr<PowerUpStringTexture> m_texture;
	// フォーマットから作成するのでキャッシュを使わない
	std::shared_ptr<Texture> m_guidelineTexture;
	std::vector<uint8_t> m_rgbaBuffer;
	bool m_ready = false;
	bool m_textureDirty = false;
	int textPixelWidth;
	int textPixelHeight;

	

	HDC m_memoryDC = nullptr;
	HBITMAP m_bitmap = nullptr;
	HFONT m_font = nullptr;
	HGDIOBJ m_prevBitmap = nullptr;
	HGDIOBJ m_prevFont = nullptr;
	void* m_rawBits = nullptr;

	DirectX::XMFLOAT4X4 m_identityMatrix;

};
