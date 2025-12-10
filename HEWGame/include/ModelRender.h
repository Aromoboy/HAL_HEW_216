#pragma once

#include <memory>
#include <unordered_set>
#include <wrl/client.h>

#include "ModelDX.h"
#include "ICamera.h"
#include "Transform.h"
#include "Shader.h"

class Texture2D;

class ModelRender
{
public:
    ModelRender();
    ~ModelRender();

    void Draw(Render::Model& model, Transform transform, const CameraSpace::ICamera* camera);

    static void EnsureDefaultShaders();
    static std::shared_ptr<VertexShader> s_defaultVS;
    static std::shared_ptr<PixelShader> s_defaultPS;

private:
    void MeshDraw(MeshDX&);

    std::unordered_set<int> m_warnedMissingSrvMaterials;
    std::shared_ptr<Texture2D> m_fallbackTexture;
    std::shared_ptr<MaterialDX> m_defaultMaterial;

    static Microsoft::WRL::ComPtr<ID3D11InputLayout> s_defaultInputLayout;
};

namespace Render
{
    using ModelRenderer = ::ModelRender;
}
