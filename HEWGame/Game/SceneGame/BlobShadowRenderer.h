#pragma once

#include <memory>
#include <optional>

#include "ICamera.h"
#include "SceneGameConfigTypes.h"
#include "Entities/PhysicalEntity.h"

#include <DirectXMath.h>

class MeshDX;
class VertexShader;
class PixelShader;

class BlobShadowRenderer
{
public:
    BlobShadowRenderer();
    ~BlobShadowRenderer();

    void SetSettings(const SceneGameConfig::BlobShadowSettings& settings);
    void DrawShadow(Entities::PhysicalEntity& entity,
                    const CameraSpace::ICamera& camera,
                    float receiverY,
                    std::optional<float> receiverHeight = std::nullopt);

private:
    struct ShadowParams
    {
        DirectX::XMFLOAT4 color{ 0.0f, 0.0f, 0.0f, 0.0f };
        float softness = 0.0f;
        float padding[3]{ 0.0f, 0.0f, 0.0f };
    };

    bool EnsureReady();
    void DrawQuad(DirectX::XMFLOAT4X4 wvp[3], const ShadowParams& params);

    SceneGameConfig::BlobShadowSettings m_settings{};
    std::shared_ptr<MeshDX> m_mesh;
    std::shared_ptr<VertexShader> m_vs;
    std::shared_ptr<PixelShader> m_ps;
    bool m_ready = false;
};
