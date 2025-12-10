#pragma once

#include <map>
#include <memory>
#include <wrl/client.h>
#include <DirectXMath.h>

#include "ICamera.h"
#include "Transform.h"

class MeshDX;
class ModelRender;
class Texture2D;

class UnifiedQuadRenderer
{
public:
    struct DrawParams
    {
        std::shared_ptr<Texture2D> texture;
        DirectX::XMFLOAT2 positionPx{ 0.0f, 0.0f }; // top-left in screen space
        DirectX::XMFLOAT2 sizePx{ 0.0f, 0.0f };
        DirectX::XMFLOAT2 uvStart{ 0.0f, 0.0f };
        DirectX::XMFLOAT2 uvSize{ 1.0f, 1.0f };
        DirectX::XMFLOAT4 tint{ 1.0f, 1.0f, 1.0f, 1.0f };
        bool enableBlending{ true };
        bool disableDepth{ true };
    };

    UnifiedQuadRenderer();
    ~UnifiedQuadRenderer();

    bool Draw(const DrawParams& params);
    bool TryGetViewportSize(float& width, float& height) const;

private:
    struct UvKey
    {
        float startX{};
        float startY{};
        float sizeX{};
        float sizeY{};

        bool operator<(const UvKey& other) const;
    };

    class ScreenSpaceCamera : public CameraSpace::ICamera
    {
    public:
        void UpdateProjection(float width, float height);
        const DirectX::XMFLOAT4X4& GetView() const override { return m_view; }
        const DirectX::XMFLOAT4X4& GetProjection() const override { return m_proj; }

    protected:
        void RecalculateView() override {}

    private:
        DirectX::XMFLOAT4X4 m_view{};
        DirectX::XMFLOAT4X4 m_proj{};
        float m_width = 0.0f;
        float m_height = 0.0f;
    };

    class RenderStateGuard
    {
    public:
        RenderStateGuard(bool disableDepth, bool enableBlend);
        ~RenderStateGuard();

    private:
        bool m_restoreDepth = false;
        bool m_restoreBlend = false;
        UINT m_prevStencilRef = 0;
        UINT m_prevSampleMask = 0;
        FLOAT m_prevBlendFactor[4]{};
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_prevDepthState;
        Microsoft::WRL::ComPtr<ID3D11BlendState> m_prevBlendState;
    };

    Transform BuildTransform(const DrawParams& params) const;
    std::shared_ptr<MeshDX> GetQuadMesh(const DirectX::XMFLOAT2& uvStart, const DirectX::XMFLOAT2& uvSize);

    std::unique_ptr<ModelRender> m_modelRenderer;
    ScreenSpaceCamera m_camera;
    std::map<UvKey, std::weak_ptr<MeshDX>> m_quadCache;
};

