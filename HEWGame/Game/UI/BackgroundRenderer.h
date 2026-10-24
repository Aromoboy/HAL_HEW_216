#pragma once

#include <memory>
#include <string>
#include <vector>
#include <array>

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
        float baseOffset01 = 0.0f;
        bool scrollOffsetInPixels = false;
        DirectX::XMFLOAT4 tintColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    std::vector<BackgroundLayer> layers;
    bool layeredScene = false;
    std::array<float, 4> transitionHeights{ 5.0f, 15.0f, 25.0f, 35.0f };
    float minimumTransitionSeconds = 13.0f;
};

class BackgroundRenderer
{
public:
    BackgroundRenderer();
    ~BackgroundRenderer();

    bool Initialize(const BackgroundRendererConfig& config);
    void Shutdown();
    void Update(float dt, float scrollOffset);
    // ミカ：分割背景は画面比率に合わせ、一枚背景はUVの表示範囲で補正する
    void Draw(float scrollOffset, float pixelsPerUnit) const;

private:
    struct LayeredScene;
    std::unique_ptr<LayeredScene> m_scene;
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
