// ============================================================================
// 概要: HudRenderer に関するインターフェースやデータ構造を宣言し、include から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

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

private:
    class DynamicTexture;

    bool CreateTextResources();
    void DestroyTextResources();
    void UpdateBitmap(const HudDisplayData& data);
    void UploadTexture();
    void RenderSprite(const HudDisplayData& data);
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

#endif // 備考: HEWGAME_HUD_RENDERER_H に関する説明を日本語で補足している。
