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

enum PowerUpTextureType
{
    NodeName,
    NodeInfomation,
    NodeLevel,
    NodeCost,
    MaxNodeType,
};

class PowerUpStringTexture
{
public:
    PowerUpStringTexture();

    HRESULT Create(DXGI_FORMAT format, UINT width, UINT height);
    void UpdatePixels(const void* data, UINT pitch);
    void SetScale(float2 a_scale);
    float2 GetScale() const;
    UINT GetWidth() const;
    UINT GetHeight() const;
    ID3D11ShaderResourceView* Get() const;
    std::shared_ptr<Texture2D> GetTexture() const;

private:
    std::shared_ptr<Texture2D> m_texture;
    float2 m_stringTextureScale{};
    UINT m_width = 0u;
    UINT m_height = 0u;
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
            : texture(std::move(a_texture))
            , stringTexture(std::move(a_stringTexture))
            , drawPosition(a_drawPosition)
            , drawScale(a_drawScale)
            , dirty(a_dirty)
            , powerUpName(std::move(a_powerUpName))
            , powerUpInfomation(std::move(a_powerUpInfomation))
            , level(a_level)
            , cost(a_cost)
        {
        }
    };

    struct UVRect
    {
        float2 start{ 0.0f, 0.0f };
        float2 size{ 1.0f, 1.0f };
    };

    PowerUpRenderer(TextureCache& cache);
    ~PowerUpRenderer();

    bool Initialize(const PowerUpHudLayoutConfig& config);
    void Shutdown();

    void DrawHud(const CameraSpace::ICamera& camera);
    void DrawCursor(std::shared_ptr<Texture2D> texture, float2 position, float2 scale, const CameraSpace::ICamera& camera);
    void DrawNode(const NodeRenderData& data, const CameraSpace::ICamera& camera);

    void DrawSprite(
        const std::shared_ptr<Texture2D>& texture,
        const UVRect& uvRect,
        const float2& position,
        const float2& scale,
        const DirectX::XMFLOAT4& tint,
        const CameraSpace::ICamera& camera);

private:
    std::shared_ptr<MeshDX> GetQuadMesh();
    void EnsureQuadMesh();

    std::shared_ptr<Render::Model> BuildModel(
        const std::shared_ptr<MaterialDX>& material,
        const std::shared_ptr<MeshDX>& mesh) const;

    Transform MakeTransform(const float2& position, const float2& scale) const;

    TextureCache& m_cache;
    PowerUpHudLayoutConfig m_config{};

    std::unique_ptr<ModelRender> m_modelRenderer;

    std::shared_ptr<MeshDX> m_quadMeshStrong;
    std::weak_ptr<MeshDX>    m_quadMeshWeak;

    std::shared_ptr<Texture2D> m_cost;
    std::shared_ptr<Texture2D> m_iconFrame;
    std::shared_ptr<Texture2D> m_backFrame;
};
