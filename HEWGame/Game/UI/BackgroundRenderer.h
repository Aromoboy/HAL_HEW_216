#pragma once

#include <memory>
#include <string>
#include <vector>

#include <DirectXMath.h>

class Texture;

struct BackgroundRendererConfig
{
    enum class SamplingMode
    {
        Wrap,
        Clamp
    };

    struct BackgroundLayer
    {
        std::string texturePath;
        float parallaxFactor = 0.0f;
        bool invertScroll = true;
        SamplingMode samplingMode = SamplingMode::Wrap;
        DirectX::XMFLOAT4 tintColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    std::vector<BackgroundLayer> layers;
};

class BackgroundRenderer
{
public:
    BackgroundRenderer();
    ~BackgroundRenderer();

    bool Initialize(const BackgroundRendererConfig& config);
    void Shutdown();
    // Aspect correction is handled via the UV window; sprite size stays at 2x2 in clip space.
    void Draw(float scrollOffset, float pixelsPerUnit) const;

private:
    std::unique_ptr<Texture> LoadTexture(const std::string& path) const;

    struct LoadedLayer
    {
        BackgroundRendererConfig::BackgroundLayer config;
        std::unique_ptr<Texture> texture;
    };

    BackgroundRendererConfig m_config{};
    std::vector<LoadedLayer> m_layers;
    bool m_ready = false;
    DirectX::XMFLOAT4X4 m_identityMatrix;
};
