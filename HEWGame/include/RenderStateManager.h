#pragma once

#include <wrl/client.h>

#include "DirectX.h"

class RenderStateGuard
{
public:
    RenderStateGuard();
    RenderStateGuard(const RenderStateGuard&) = delete;
    RenderStateGuard& operator=(const RenderStateGuard&) = delete;
    ~RenderStateGuard();

    void ApplyDepthTest(bool enabled);
    void ApplyBlendMode(BlendMode mode);

private:
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_prevDepthState;
    UINT m_prevStencilRef = 0;

    Microsoft::WRL::ComPtr<ID3D11BlendState> m_prevBlendState;
    FLOAT m_prevBlendFactor[4]{};
    UINT m_prevSampleMask = 0xffffffff;

    bool m_depthStateCaptured = false;
    bool m_blendStateCaptured = false;
};
