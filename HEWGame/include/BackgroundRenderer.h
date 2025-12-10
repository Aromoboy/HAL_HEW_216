#pragma once

#include <memory>
#include <string>

#include <DirectXMath.h>

class MaterialDX;
class MeshDX;
class ModelRender;
class Texture2D;
namespace Render
{
    class Model;
}

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
    std::shared_ptr<MeshDX> m_quadMesh;
    std::shared_ptr<MaterialDX> m_material;
    std::unique_ptr<Render::Model> m_model;
    std::unique_ptr<ModelRender> m_modelRenderer;
    bool m_ready = false;
};
