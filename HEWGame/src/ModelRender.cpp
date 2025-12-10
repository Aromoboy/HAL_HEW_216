#include "ModelRender.h"

#include "Defines.h"
#include "Vector.h"
#include "DirectX.h"
#include "Debug.h"
#include "MatrixUtil.h"
#include "TextureCache.h"

#include <cstdint>
#include <array>
#include <cstring>
#include <cassert>
#include <d3dcompiler.h>

std::shared_ptr<VertexShader> ModelRender::s_defaultVS;
std::shared_ptr<PixelShader> ModelRender::s_defaultPS;
Microsoft::WRL::ComPtr<ID3D11InputLayout> ModelRender::s_defaultInputLayout;

namespace
{
    class ScopedRenderState
    {
    public:
        ScopedRenderState()
        {
            ID3D11DeviceContext* context = GetContext();
            if (!context)
            {
                return;
            }

            context->OMGetDepthStencilState(m_prevDepthState.GetAddressOf(), &m_prevStencilRef);
            context->OMGetBlendState(m_prevBlendState.GetAddressOf(), m_prevBlendFactor, &m_prevSampleMask);
            context->RSGetState(m_prevRasterizerState.GetAddressOf());
            context->PSGetSamplers(0, 1, m_prevSampler.GetAddressOf());

            SetDepthTest(true);
            SetBlendMode(BLEND_NONE);
            SetCullingMode(D3D11_CULL_BACK);
            SetSamplerState(SAMPLER_LINEAR);
        }

        ~ScopedRenderState()
        {
            ID3D11DeviceContext* context = GetContext();
            if (!context)
            {
                return;
            }

            context->OMSetDepthStencilState(m_prevDepthState.Get(), m_prevStencilRef);
            context->OMSetBlendState(m_prevBlendState.Get(), m_prevBlendFactor, m_prevSampleMask);
            context->RSSetState(m_prevRasterizerState.Get());

            ID3D11SamplerState* sampler = m_prevSampler.Get();
            context->PSSetSamplers(0, 1, &sampler);
        }

    private:
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_prevDepthState;
        Microsoft::WRL::ComPtr<ID3D11BlendState> m_prevBlendState;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_prevRasterizerState;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> m_prevSampler;
        FLOAT m_prevBlendFactor[4]{};
        UINT m_prevSampleMask = 0xffffffff;
        UINT m_prevStencilRef = 0;
    };
}

void ModelRender::EnsureDefaultShaders()
{
    if (s_defaultVS && s_defaultPS && s_defaultInputLayout)
    {
        return;
    }

    const char* g_defaultVS = R"(
cbuffer VSConstants : register(b0)
{
    float4x4 gWorldViewProj;
};

struct VSInput {
    float3 pos : POSITION;
    float2 uv  : TEXCOORD0;
};

struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.pos = mul(float4(input.pos, 1.0f), gWorldViewProj);
    output.uv  = input.uv;
    return output;
}
)";

    const char* g_defaultPS = R"(
Texture2D tex0 : register(t0);
SamplerState sam0 : register(s0);

cbuffer PSConstants : register(b0)
{
    float4 diffuseColor;
};

float4 main(float2 uv : TEXCOORD0) : SV_TARGET
{
    float4 texColor = tex0.Sample(sam0, uv);
    return texColor * diffuseColor;
}
)";

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> vsError;
    HRESULT hrCompileVS = D3DCompile(
        g_defaultVS,
        strlen(g_defaultVS),
        nullptr,
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
        0,
        vsBlob.GetAddressOf(),
        vsError.GetAddressOf());

    if (FAILED(hrCompileVS))
    {
        DebugMessage("[ModelRender] Failed to compile default vertex shader.\n");
        return;
    }

    s_defaultVS = std::make_shared<VertexShader>();
    HRESULT hrVS = s_defaultVS->Compile(g_defaultVS);
    if (FAILED(hrVS))
    {
        DebugMessage("[ModelRender] Failed to compile default vertex shader.\n");
        s_defaultVS.reset();
        return;
    }

    if (!s_defaultInputLayout)
    {
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        HRESULT hrLayout = GetDevice()->CreateInputLayout(
            layout,
            static_cast<UINT>(std::size(layout)),
            vsBlob->GetBufferPointer(),
            vsBlob->GetBufferSize(),
            s_defaultInputLayout.GetAddressOf());

        if (FAILED(hrLayout))
        {
            DebugMessage("[ModelRender] Failed to create default input layout.\n");
            s_defaultInputLayout.Reset();
        }
    }

    s_defaultPS = std::make_shared<PixelShader>();
    HRESULT hrPS = s_defaultPS->Compile(g_defaultPS);
    if (FAILED(hrPS))
    {
        DebugMessage("[ModelRender] Failed to compile default pixel shader.\n");
        s_defaultPS.reset();
        s_defaultVS.reset();
        return;
    }
}

ModelRender::ModelRender()
{
    EnsureDefaultShaders();

    auto& cache = TextureCache::Instance();
    constexpr const char* kFallbackKey = "ModelRenderFallbackWhite";
    m_fallbackTexture = cache.GetTexture(kFallbackKey);
    if (!m_fallbackTexture)
    {
        static const uint32_t kWhitePixel = 0xFFFFFFFFu;
        m_fallbackTexture = cache.LoadFromMemory(
            kFallbackKey,
            GetDevice(),
            reinterpret_cast<const uint8_t*>(&kWhitePixel),
            sizeof(kWhitePixel),
            1,
            1,
            "R8G8B8A8");

        if (!m_fallbackTexture)
        {
            DebugMessage("[ModelRender] Failed to create fallback texture for missing SRVs");
        }
    }

    if (m_fallbackTexture)
    {
        m_defaultMaterial = std::make_shared<MaterialDX>(
            DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
            DirectX::XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f),
            DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
            m_fallbackTexture);
    }
    else
    {
        m_defaultMaterial = std::make_shared<MaterialDX>(
            DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
            DirectX::XMFLOAT4(0.3f, 0.3f, 0.3f, 0.0f),
            DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
            nullptr);
    }
}

ModelRender::~ModelRender() = default;

void ModelRender::Draw(Render::Model& a_model, Transform transform, const CameraSpace::ICamera* camera)
{
    if (!camera)
    {
        return;
    }

    if (!s_defaultVS || !s_defaultPS)
    {
        DebugMessage("[ModelRender] Default shaders are not initialized.\n");
        return;
    }

    const ScopedRenderState scopedState;

    struct VSConstants
    {
        DirectX::XMFLOAT4X4 worldViewProj;
    };

    VSConstants vsConstants{};
    const float3 radRotation = { transform.rotation.x * RAD, transform.rotation.y * RAD, transform.rotation.z * RAD };
    DirectX::XMFLOAT4X4 world = MathUtil::MakeWorldMatrix(transform.position, transform.scale, radRotation);
    DirectX::XMFLOAT4X4 view = camera->GetView();
    DirectX::XMFLOAT4X4 proj = camera->GetProjection();

    DirectX::XMMATRIX worldMatrix = DirectX::XMLoadFloat4x4(&world);
    DirectX::XMMATRIX viewMatrix = DirectX::XMLoadFloat4x4(&view);
    DirectX::XMMATRIX projMatrix = DirectX::XMLoadFloat4x4(&proj);
    DirectX::XMMATRIX wvpMatrix = DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(worldMatrix, viewMatrix), projMatrix);
    DirectX::XMStoreFloat4x4(&vsConstants.worldViewProj, wvpMatrix);

    for (int i = 0; i < a_model.GetMeshCount(); i++)
    {
        std::shared_ptr<MeshDX> mesh = a_model.GetMesh(i).lock();
        if (!mesh)
        {
            continue;
        }

        const unsigned int materialCount = a_model.GetMaterialCount();
        const int materialId = mesh->GetMaterialID();
        std::shared_ptr<MaterialDX> material;

        if (materialId >= 0 && static_cast<unsigned int>(materialId) < materialCount)
        {
            material = a_model.GetMaterial(materialId).lock();
        }

        if (material && !material->GetTexture() && m_warnedMissingSrvMaterials.emplace(materialId).second)
        {
            DebugMessage("[ModelRender] Material has NO SRV, fallback to white");
        }

        if (material && !material->GetTexture() && m_fallbackTexture)
        {
            material = std::make_shared<MaterialDX>(
                material->GetDiffuse(),
                material->GetAmbient(),
                material->GetSpecular(),
                m_fallbackTexture);
        }

        std::shared_ptr<MaterialDX> effectiveMaterial = material ? material : m_defaultMaterial;

        ID3D11ShaderResourceView* srvRef = effectiveMaterial
            ? effectiveMaterial->GetTexture()
            : (m_fallbackTexture ? m_fallbackTexture->Get() : nullptr);

        const bool expectsFallbackTexture = m_fallbackTexture && (!material || !material->GetTexture());
        assert(!expectsFallbackTexture || srvRef);

        struct PSConstants
        {
            DirectX::XMFLOAT4 diffuseColor;
        };

        PSConstants psConstants{ effectiveMaterial->GetDiffuse() };

        s_defaultVS->WriteBuffer(0, &vsConstants);
        s_defaultPS->WriteBuffer(0, &psConstants);
        s_defaultPS->SetTexture(0, srvRef);

        s_defaultPS->Bind();
        s_defaultVS->Bind();

        MeshDraw(*mesh);
    }
}

//void ModelRender::Draw(ModelDX& a_model, const CameraSpace::ICamera* camera)
//{
//
//}

void ModelRender::MeshDraw(MeshDX& a_mesh)
{
        ID3D11DeviceContext* pContext = GetContext();
        if (!s_defaultInputLayout)
        {
            DebugMessage("[ModelRender] Default input layout is not initialized.\n");
            return;
        }

        const Core::VertexBuffer& vertexBuffer = a_mesh.GetVertexBuffer();

        pContext->IASetInputLayout(s_defaultInputLayout.Get());
        pContext->IASetPrimitiveTopology(a_mesh.GetTopology());

        UINT stride = MeshDX::VertexStride;
        UINT offset = 0;
        vertexBuffer.Bind(0, stride, offset);

        if (a_mesh.HasIndexBuffer())
        {
                const Core::IndexBuffer* indexBuffer = a_mesh.GetIndexBuffer();
                if (indexBuffer)
                {
                        indexBuffer->Bind();
                        pContext->DrawIndexed(a_mesh.GetIndexCount(), 0, 0);
                }
        }
        else
        {
                pContext->Draw(vertexBuffer.GetCount(), 0);
        }
}
