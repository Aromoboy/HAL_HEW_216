#include "ModelRender.h"

#include "Defines.h"
#include "Vector.h"
#include "DirectX.h"
#include "Debug.h"
#include "MatrixUtil.h"
#include "TextureCache.h"

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#include <cstring>
#include <array>

#include <cstdint>

namespace
{
    struct ModelShaderResources
    {
        ID3D11VertexShader* vertexShader{ nullptr };
        ID3D11PixelShader* pixelShader{ nullptr };
        ID3D11InputLayout* inputLayout{ nullptr };
        ID3D11Buffer* wvpBuffer{ nullptr };
        ID3D11Buffer* materialBuffer{ nullptr };
    };

    ModelShaderResources g_modelShaders;

    struct ModelWvp
    {
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 proj;
    };

    struct MaterialParams
    {
        DirectX::XMFLOAT4 diffuse;
        DirectX::XMFLOAT4 ambient;
        DirectX::XMFLOAT4 specular;
    };

    void ReleaseModelShaders()
    {
        SAFE_RELEASE(g_modelShaders.vertexShader);
        SAFE_RELEASE(g_modelShaders.pixelShader);
        SAFE_RELEASE(g_modelShaders.inputLayout);
        SAFE_RELEASE(g_modelShaders.wvpBuffer);
        SAFE_RELEASE(g_modelShaders.materialBuffer);
    }

    bool CreateConstantBuffer(UINT byteWidth, ID3D11Buffer** buffer)
    {
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = (byteWidth + 15u) & ~15u;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        return SUCCEEDED(GetDevice()->CreateBuffer(&desc, nullptr, buffer));
    }

    bool CompileShader(const char* source, const char* target, ID3DBlob** blob)
    {
        return SUCCEEDED(D3DCompile(
            source,
            strlen(source),
            nullptr,
            nullptr,
            nullptr,
            "main",
            target,
            D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
            0,
            blob,
            nullptr));
    }
}

void ModelRender::EnsureDefaultShaders()
{
    if (g_modelShaders.vertexShader && g_modelShaders.pixelShader && g_modelShaders.inputLayout && g_modelShaders.wvpBuffer && g_modelShaders.materialBuffer)
    {
        return;
    }

    ReleaseModelShaders();

    const char* kDefaultVS = R"EOT(
struct VS_IN {
        float3 pos : POSITION;
        float2 uv : TEXCOORD0;
};
struct VS_OUT {
        float4 pos : SV_POSITION;
        float2 uv : TEXCOORD0;
};
cbuffer WVP : register(b0) {
        float4x4 world;
        float4x4 view;
        float4x4 proj;
};
VS_OUT main(VS_IN vin) {
        VS_OUT vout;
        float4 worldPos = mul(float4(vin.pos, 1.0f), world);
        float4 viewPos = mul(worldPos, view);
        vout.pos = mul(viewPos, proj);
        vout.uv = vin.uv;
        return vout;
})EOT";

    const char* kDefaultPS = R"EOT(
struct PS_IN {
        float4 pos : SV_POSITION;
        float2 uv : TEXCOORD0;
};
cbuffer Material : register(b0) {
        float4 diffuse;
        float4 ambient;
        float4 specular;
};
Texture2D tex : register(t0);
SamplerState samp : register(s0);
float4 main(PS_IN pin) : SV_TARGET
{
        float4 sampled = tex.Sample(samp, pin.uv);
        float3 base = sampled.rgb * diffuse.rgb;
        float3 lit = saturate(base + ambient.rgb);
        return float4(lit, sampled.a * diffuse.a);
})EOT";

    ID3DBlob* vsBlob = nullptr;
    if (!CompileShader(kDefaultVS, "vs_5_0", &vsBlob))
    {
        SAFE_RELEASE(vsBlob);
        return;
    }

    if (FAILED(GetDevice()->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_modelShaders.vertexShader)))
    {
        SAFE_RELEASE(vsBlob);
        return;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    const UINT layoutCount = static_cast<UINT>(std::size(layout));
    if (FAILED(GetDevice()->CreateInputLayout(layout, layoutCount, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_modelShaders.inputLayout)))
    {
        SAFE_RELEASE(vsBlob);
        return;
    }

    SAFE_RELEASE(vsBlob);

    ID3DBlob* psBlob = nullptr;
    if (!CompileShader(kDefaultPS, "ps_5_0", &psBlob))
    {
        SAFE_RELEASE(psBlob);
        return;
    }

    if (FAILED(GetDevice()->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_modelShaders.pixelShader)))
    {
        SAFE_RELEASE(psBlob);
        return;
    }

    SAFE_RELEASE(psBlob);

    if (!CreateConstantBuffer(sizeof(ModelWvp), &g_modelShaders.wvpBuffer))
    {
        ReleaseModelShaders();
        return;
    }

    if (!CreateConstantBuffer(sizeof(MaterialParams), &g_modelShaders.materialBuffer))
    {
        ReleaseModelShaders();
    }
}

ModelRender::ModelRender()
{
    EnsureDefaultShaders();

    if (!g_modelShaders.vertexShader || !g_modelShaders.pixelShader)
    {
        DebugMessage("[ModelRender] Failed to initialize default shaders");
    }

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

    if (!g_modelShaders.vertexShader || !g_modelShaders.pixelShader || !g_modelShaders.inputLayout || !g_modelShaders.wvpBuffer || !g_modelShaders.materialBuffer)
    {
        return;
    }

    ID3D11DeviceContext* context = GetContext();
    if (!context)
    {
        return;
    }

    ModelWvp wvp = {};
    wvp.view = camera->GetView();
    wvp.proj = camera->GetProjection();

    const float3 radRotation = { transform.rotation.x * RAD, transform.rotation.y * RAD, transform.rotation.z * RAD };
    wvp.world = MathUtil::MakeWorldMatrix(transform.position, transform.scale, radRotation);

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

        struct MaterialParams
        {
            DirectX::XMFLOAT4 diffuse;
            DirectX::XMFLOAT4 ambient;
            DirectX::XMFLOAT4 specular;
        };

        ID3D11ShaderResourceView* srvRef = effectiveMaterial
            ? effectiveMaterial->GetTexture()
            : (m_fallbackTexture ? m_fallbackTexture->Get() : nullptr);

        MaterialParams params{ effectiveMaterial->GetDiffuse(), effectiveMaterial->GetAmbient(), effectiveMaterial->GetSpecular() };

        context->UpdateSubresource(g_modelShaders.wvpBuffer, 0, nullptr, &wvp, 0, 0);
        context->UpdateSubresource(g_modelShaders.materialBuffer, 0, nullptr, &params, 0, 0);
        context->PSSetShaderResources(0, 1, &srvRef);
        context->VSSetConstantBuffers(0, 1, &g_modelShaders.wvpBuffer);
        context->PSSetConstantBuffers(0, 1, &g_modelShaders.materialBuffer);
        context->IASetInputLayout(g_modelShaders.inputLayout);
        context->VSSetShader(g_modelShaders.vertexShader, nullptr, 0);
        context->PSSetShader(g_modelShaders.pixelShader, nullptr, 0);

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
        UINT offset = 0;

        const Core::VertexBuffer& vertexBuffer = a_mesh.GetVertexBuffer();
        vertexBuffer.Bind(0, offset);

        pContext->IASetPrimitiveTopology(a_mesh.GetTopology());

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
