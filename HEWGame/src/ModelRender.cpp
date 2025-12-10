#include "ModelRender.h"

#include "Defines.h"
#include "Vector.h"
#include "DirectX.h"
#include "Debug.h"
#include "MatrixUtil.h"
#include "TextureCache.h"
#include "WorldBuffer.h"

#include <cstdint>

std::shared_ptr<VertexShader> ModelRender::s_defaultVS;
std::shared_ptr<PixelShader> ModelRender::s_defaultPS;

void ModelRender::EnsureDefaultShaders()
{
    if (s_defaultVS && s_defaultPS)
    {
        return;
    }

    const char* kDefaultVS = R"EOT(
struct VS_IN {
        float3 pos : POSITION;
        float2 uv : TEXCOORD0;
        float3 normal : NORMAL0;
};
struct VS_OUT {
        float4 pos : SV_POSITION;
        float3 normal : NORMAL0;
        float2 uv : TEXCOORD0;
};
cbuffer WVP : register(b0) {
        float4x4 world;
        float4x4 view;
        float4x4 proj;
        float4 normalBasis[3];
};
VS_OUT main(VS_IN vin) {
        VS_OUT vout;
        float4 worldPos = mul(float4(vin.pos, 1.0f), world);
        float4 viewPos = mul(worldPos, view);
        vout.pos = mul(viewPos, proj);
        vout.uv = vin.uv;
        float3x3 normalMat = float3x3(normalBasis[0].xyz, normalBasis[1].xyz, normalBasis[2].xyz);
        vout.normal = normalize(mul(vin.normal, normalMat));
        return vout;
})EOT";

    const char* kDefaultPS = R"EOT(
struct PS_IN {
        float4 pos : SV_POSITION;
        float3 normal : NORMAL0;
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
        float3 n = normalize(pin.normal);
        float3 lightDir = normalize(float3(-1.0f, -1.0f, -1.0f));
        float lambert = saturate(dot(n, -lightDir));
        float4 sampled = tex.Sample(samp, pin.uv);
        float3 base = sampled.rgb * diffuse.rgb;
        float3 lit = lerp(ambient.rgb * diffuse.rgb, base, lambert);
        return float4(lit, sampled.a * diffuse.a);
})EOT";

    s_defaultVS = std::make_shared<VertexShader>();
    s_defaultVS->Compile(kDefaultVS);
    s_defaultPS = std::make_shared<PixelShader>();
    s_defaultPS->Compile(kDefaultPS);
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

    WorldBuffer wvp = {};
    wvp.view = camera->GetView();
    wvp.proj = camera->GetProjection();

    const float3 radRotation = { transform.rotation.x * RAD, transform.rotation.y * RAD, transform.rotation.z * RAD };
    wvp.world = MathUtil::MakeWorldMatrix(transform.position, transform.scale, radRotation);
    wvp.normalBasis[0] = DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 0.0f);
    wvp.normalBasis[1] = DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
    wvp.normalBasis[2] = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 0.0f);

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

        s_defaultVS->WriteBuffer(0, &wvp);
        s_defaultPS->WriteBuffer(0, &params);
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
