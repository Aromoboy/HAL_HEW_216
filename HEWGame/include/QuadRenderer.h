#pragma once

#include <memory>

#include <DirectXMath.h>

#include "Transform.h"

class MaterialDX;
class MeshDX;
class ModelRender;
class Texture2D;
namespace CameraSpace
{
    class ICamera;
}

class QuadRenderer
{
public:
    QuadRenderer();
    explicit QuadRenderer(std::shared_ptr<MaterialDX> material);
    ~QuadRenderer();

    void SetMaterial(std::shared_ptr<MaterialDX> material);
    bool SetTexture(const std::shared_ptr<Texture2D>& texture, const DirectX::XMFLOAT4& tint = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

    std::shared_ptr<MaterialDX> GetMaterial() const;

    void Draw(ModelRender& renderer, const Transform& transform, const CameraSpace::ICamera* camera) const;

private:
    static std::shared_ptr<MeshDX> GetSharedQuadMesh();

    std::shared_ptr<MaterialDX> m_material;
};

