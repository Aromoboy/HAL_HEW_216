#pragma once

#include <memory>
#include <string>

#include <DirectXMath.h>
#include "TextureCache.h"

struct BackgroundRendererConfig
{
    std::string texturePath;
    DirectX::XMFLOAT4 tintColor{ 1.0f, 1.0f, 1.0f, 1.0f };
};

class BackgroundRenderer
{
public:
    BackgroundRenderer();
    ~BackgroundRenderer();

    bool Initialize(const BackgroundRendererConfig& config);
    void Shutdown();
    void Draw() const;

private:
    bool LoadTexture(const std::string& path);

    BackgroundRendererConfig m_config{};
    std::shared_ptr<Texture2D> m_texture;
    bool m_ready = false;
    DirectX::XMFLOAT4X4 m_identityMatrix;
};
