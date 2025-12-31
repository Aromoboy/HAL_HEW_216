// ゲームの背景表示を描く役目を持つ
// シーンゲームの描画から呼ばれる

#pragma once

#include <memory>
#include <string>

#include <DirectXMath.h>

class Texture;

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
    std::unique_ptr<Texture> m_texture;
    bool m_ready = false;
    DirectX::XMFLOAT4X4 m_identityMatrix;
};
