#pragma once

#include "MeshDX.h"
#include "Vector.h"
#include "TextureDX.h"

#include <Shader.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <DirectXMath.h>
#if defined(_WIN32)
#include <windows.h>
#include <array>

#endif


enum ResultSprite
{
    BACKGROUND,
    MAX_RESULT_SPRITE,
};

class Texture;

struct ResultDisplayData
{
    float remainingSeconds = 0.0f;
    float currentHeight = 0.0f;
    float peakHeight = 0.0f;
    int score = 0;
    bool timeUp = false;
    float moveSpeedMultiplier = 1.0f;
    float moveSpeedUnitsPerSecond = 0.0f;
    int sweetsCount = 0;
};

struct ResultLayoutConfig
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


class ResultRenderer
{
public:
    ResultRenderer();
    ~ResultRenderer();

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
        {
        }
    };

    bool Initialize(const ResultLayoutConfig& config);
    void Shutdown();
    void Draw(const ResultDisplayData& data);
    void Draw(std::shared_ptr<Texture2D> a_texture, float2 a_pos, float2 a_size);

private:
    class DynamicTexture;

    bool CreateTextResources();
    bool CreateGuidelineTexture();
    void DestroyTextResources();
    void UpdateBitmap(const ResultDisplayData& data);
    void UploadTexture();
    void RenderFream(const ResultDisplayData& data);
    void RenderSprite(const ResultDisplayData& data);
    bool TryGetViewportSize(float& width, float& height) const;
    std::string BuildHudString(const ResultDisplayData& data) const;
    void DrawSprite(std::shared_ptr<Texture2D> texture, const DirectX::XMFLOAT4X4& worldMatrix, const DirectX::XMFLOAT4X4& viewMatrix, const DirectX::XMFLOAT4X4& projMatrix, const DrawSpriteData& drawData);
    void MeshDraw();

    ResultLayoutConfig m_config{};
    std::unique_ptr<DynamicTexture> m_texture;
    std::array<std::unique_ptr<Texture>, MAX_RESULT_SPRITE> m_resultTexture;
    std::vector<uint8_t> m_rgbaBuffer;
    bool m_ready = false;
    bool m_textureDirty = false;

    HDC m_memoryDC = nullptr;
    HBITMAP m_bitmap = nullptr;
    HFONT m_font = nullptr;
    HGDIOBJ m_prevBitmap = nullptr;
    HGDIOBJ m_prevFont = nullptr;
    void* m_rawBits = nullptr;

    std::shared_ptr<MeshDX> m_spriteMesh;
    std::shared_ptr<Shader> vs;
    std::shared_ptr<Shader> ps;

    DirectX::XMFLOAT4X4 m_identityMatrix;
};
