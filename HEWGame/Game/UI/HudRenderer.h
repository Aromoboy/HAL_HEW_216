#ifndef HEWGAME_HUD_RENDERER_H
#define HEWGAME_HUD_RENDERER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <DirectXMath.h>
#if defined(_WIN32)
#include <windows.h>
#endif

class Texture;

struct HudDisplayData
{
    float remainingSeconds = 0.0f;
    float currentHeight = 0.0f;
    float peakHeight = 0.0f;
    int score = 0;
    bool timeUp = false;
    float moveSpeedMultiplier = 1.0f;
    float moveSpeedUnitsPerSecond = 0.0f;
    int sweetsCount = 0;
    bool showGlueDebug = false;
    int glueActiveFruitGlue = 0;
    int glueFrozenFruit = 0;
    float glueCreatePerSec = 0.0f;
    float glueRejectPerSec = 0.0f;
    float glueCleanupPerSec = 0.0f;
};

struct HudLayoutConfig
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

class HudRenderer
{
public:
    HudRenderer();
    ~HudRenderer();

    bool Initialize(const HudLayoutConfig& config);
    void Shutdown();
    void Draw(const HudDisplayData& data);
    void Draw();

private:
    class DynamicTexture;

    bool CreateTextResources();
    void DestroyTextResources();
    void UpdateBitmap(const HudDisplayData& data);
    void UploadTexture();
    void RenderSprite(const HudDisplayData& data);
    bool TryGetViewportSize(float& width, float& height) const;
    std::string BuildHudString(const HudDisplayData& data) const;

    HudLayoutConfig m_config{};
    std::unique_ptr<DynamicTexture> m_texture;
    std::vector<uint8_t> m_rgbaBuffer;
    bool m_ready = false;
    bool m_textureDirty = false;

    HDC m_memoryDC = nullptr;
    HBITMAP m_bitmap = nullptr;
    HFONT m_font = nullptr;
    HGDIOBJ m_prevBitmap = nullptr;
    HGDIOBJ m_prevFont = nullptr;
    void* m_rawBits = nullptr;

    DirectX::XMFLOAT4X4 m_identityMatrix;
};

#endif
