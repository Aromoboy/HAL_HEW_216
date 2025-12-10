#pragma once

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <d3d11.h>
#include "MaterialDX.h"
#include "ModelDX.h"
#include "ModelRender.h"
#include "Transform.h"
#include "Vector.h"
#include "ICamera.h"
#include "TextureCache.h"
#include "MeshDX.h"
#include "TextureDX.h"
#include "QuadRenderer.h"



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
	float guidelineThicknessPixels = 3.0f;
	float guidelinePatternPixels = 32.0f;
	DirectX::XMFLOAT4 guidelineColor{ 0.95f, 0.95f, 0.95f, 0.8f };
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

class PowerUpStringTexture
{
public:
        PowerUpStringTexture();

        HRESULT Create(DXGI_FORMAT format, UINT width, UINT height);
        void UpdatePixels(const void* data, UINT pitch);
        void SetScale(float2 a_scale);
        float2 GetScale() const;
        TextBounds& GetBound();
        const TextBounds& GetBound() const;
        UINT GetWidth() const;
        UINT GetHeight() const;
        ID3D11ShaderResourceView* Get() const;
        std::shared_ptr<Texture2D> GetTexture() const;

private:
        std::shared_ptr<Texture2D> m_texture;
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

		
		// Nodeに描画する文字列及び数値
		bool dirty;
		std::string powerUpName;
		std::string powerUpInfomation;
		float level;
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
		float a_cost)
			: texture(a_texture)
			, stringTexture(a_stringTexture)
			, drawPosition(a_drawPosition)
			, drawScale(a_drawScale)
			, dirty(a_dirty)
			, powerUpName(a_powerUpName)
			, powerUpInfomation(a_powerUpInfomation)
			, level(a_level)
			, cost(a_cost)
		{
		}
	};



	PowerUpRenderer(TextureCache& cache);
	~PowerUpRenderer();

	bool Initialize(const PowerUpHudLayoutConfig& config);
	void Shutdown();

	void DrawHud(const CameraSpace::ICamera& camera);
	void DrawCursor(std::shared_ptr<Texture2D> texture, float2 a_position, float2 a_scale, const CameraSpace::ICamera&);
	void DrawNode(const NodeRenderData& data , const CameraSpace::ICamera&);




private:
	

        struct UVRegion
        {
                float startY;
                float height;
        };


        void DrawQuad(
            std::shared_ptr<Texture2D> texture,
            const Transform& transform,
            const CameraSpace::ICamera& camera,
            const float2& uvStart = float2{ 0.0f, 0.0f },
            const float2& uvScale = float2{ 1.0f, 1.0f });

        std::unique_ptr<ModelRender> m_modelRenderer;
        QuadRenderer m_quadRenderer;

	// 描画用テクスチャ
	std::shared_ptr<Texture2D> m_cost;
	std::shared_ptr<Texture2D> m_iconFrame;
	std::shared_ptr<Texture2D> m_backFrame;


	// 文字描画用関数
	bool CreateTextResources();
	bool CreateGuidelineTexture();
	void DestroyTextResources();
	void UpdateBitmap(const NodeRenderData& data);
	void UploadTexture(const NodeRenderData& data);
        void RenderString(const NodeRenderData& data, const Transform& transform, const CameraSpace::ICamera& camera, int uvNum);
	void DetectTextBounds(TextBounds& bounds, const uint8_t* rgba, int width, int height);
	UVRegion ComputeUV(const TextBounds& bounds, int textureHeight);

	bool TryGetViewportSize(float& width, float& height) const;
	std::string BuildHudString(const NodeRenderData& data) const;


        // 各リソース
        PowerUpHudLayoutConfig m_config{};
        std::shared_ptr<PowerUpStringTexture> m_texture;
        // フォーマットから作成するのでCacheを使わない
        std::shared_ptr<Texture2D> m_guidelineTexture;
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


};