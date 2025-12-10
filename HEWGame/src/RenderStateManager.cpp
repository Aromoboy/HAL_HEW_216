#include "RenderStateManager.h"

RenderStateGuard::RenderStateGuard()
{
    if (ID3D11DeviceContext* context = GetContext())
    {
        context->OMGetDepthStencilState(m_prevDepthState.GetAddressOf(), &m_prevStencilRef);
        m_depthStateCaptured = true;

        context->OMGetBlendState(m_prevBlendState.GetAddressOf(), m_prevBlendFactor, &m_prevSampleMask);
        m_blendStateCaptured = true;
    }
}

RenderStateGuard::~RenderStateGuard()
{
    if (ID3D11DeviceContext* context = GetContext())
    {
        if (m_blendStateCaptured)
        {
            context->OMSetBlendState(m_prevBlendState.Get(), m_prevBlendFactor, m_prevSampleMask);
        }

        if (m_depthStateCaptured)
        {
            context->OMSetDepthStencilState(m_prevDepthState.Get(), m_prevStencilRef);
        }
    }
}

void RenderStateGuard::ApplyDepthTest(bool enabled)
{
    ::SetDepthTest(enabled);
}

void RenderStateGuard::ApplyBlendMode(BlendMode mode)
{
    ::SetBlendMode(mode);
}
