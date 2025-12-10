#pragma once

#include <memory>
#include <unordered_set>

#include "ModelDX.h"
#include "ICamera.h"
#include "Transform.h"

class Texture2D;

class ModelRender
{
public:
    ModelRender();
    ~ModelRender();

    void Draw(Render::Model& model, Transform transform, const CameraSpace::ICamera* camera);

private:
    static void EnsureDefaultShaders();
    void MeshDraw(MeshDX&);

    std::unordered_set<int> m_warnedMissingSrvMaterials;
    std::shared_ptr<Texture2D> m_fallbackTexture;
    std::shared_ptr<MaterialDX> m_defaultMaterial;
};

namespace Render
{
    using ModelRenderer = ::ModelRender;
}
