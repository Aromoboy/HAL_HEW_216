#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

#include <DirectXMath.h>
#include <d3d11.h>
#include <wrl/client.h>

#include "IBackgroundLayerRenderer.h"
#include "MeshBuffer.h"
#include "Shader.h"

class VideoBackgroundRenderer : public IBackgroundLayerRenderer
{
public:
    struct ShaderPaths
    {
        std::filesystem::path vertexShader;
        std::filesystem::path pixelShader;
    };

    enum class ScrollMode
    {
        Clamp,
        Wrap
    };

    VideoBackgroundRenderer();
    ~VideoBackgroundRenderer();

    bool Initialize(ID3D11Device* device,
        ID3D11DeviceContext* context,
        const ShaderPaths& shader,
        const std::string& mp4Path,
        uint32_t maxDecodeHeight = 0);
    void Update(float dt) override;
    bool IsFinished() const;
    void SetAutoLoop(bool enabled);
    void SetViewportSize(float width, float height);
    void SetScroll01(float scroll01);
    void SetBaseOffset01(float baseOffset01);
    void SetBaseOffsetPixels(float baseOffsetPixels);
    void SetScrollSpeed(float scrollSpeed);
    void SetParallaxFactor(float parallaxFactor);
    void SetScrollDirection(float dir);
    void SetZoom(float zoom);
    void SetScrollMode(ScrollMode mode);
    void SetInvertY(bool invert);
    ID3D11ShaderResourceView* GetTexture() const;
    void Draw();
    void Draw(const BackgroundLayerDrawContext& context) override;
    void Shutdown();

private:
    struct DecoderState;

    bool CreateFullScreenQuad();
    bool LoadShaders(const ShaderPaths& shader);
    bool CreateVideoTexture(uint32_t width, uint32_t height);
    bool UpdateTextureFromIYUV(const uint8_t* data, size_t dataSize);

    ID3D11Device* m_device = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    std::unique_ptr<VertexShader> m_vertexShader;
    std::unique_ptr<PixelShader> m_pixelShader;
    std::unique_ptr<MeshBuffer> m_quad;
    DirectX::XMFLOAT4X4 m_matrices[3]{};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_srv;
    std::unique_ptr<DecoderState> m_decoder;
    uint32_t m_frameWidth = 0;
    uint32_t m_frameHeight = 0;
    float m_viewportWidth = 0.0f;
    float m_viewportHeight = 0.0f;
    float m_scroll01 = 0.0f;
    float m_baseOffset01 = 0.0f;
    float m_baseOffsetPixels = 0.0f;
    float m_scrollSpeed = 1.0f;
    float m_parallaxFactor = 1.0f;
    float m_scrollDirection = 1.0f;
    float m_zoom = 1.0f;
    float m_cameraY = 0.0f;
    float m_worldScrollOffset = 0.0f;
    float m_pixelsPerUnit = 1.0f;
    bool m_invertY = false;
    ScrollMode m_scrollMode = ScrollMode::Clamp;
    bool m_ready = false;
    uint32_t m_debugScrollLogFrameCounter = 0;
    float m_lastDeadRangeInputScroll01 = 0.0f;
    float m_lastDeadRangeNormalizedY = 0.0f;
    uint32_t m_deadRangeFrameStreak = 0;
    bool m_deadRangeStateInitialized = false;
    bool m_mfStarted = false;
};
