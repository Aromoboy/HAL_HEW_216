#include "ModelRender.h"

#include "DrawMatrix.h"
#include "ShaderList.h"
#include "Defines.h"
#include "Vector.h"
#include "DirectX.h"
#include "Debug.h"
#include "TextureCache.h"

#include <cstdint>

ModelRender::ModelRender()
{
    auto& cache = TextureCache::Instance();
    constexpr const char* kFallbackKey = "ModelRenderFallbackWhite";
    m_fallbackTexture = cache.Get(kFallbackKey);
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
}

ModelRender::~ModelRender() = default;

void ModelRender::Draw(Render::Model& a_model, Transform transform, const CameraSpace::ICamera* camera)
{
    if (!camera)
    {
        return;
    }

    DirectX::XMFLOAT4X4 wvp[3];
    wvp[1] = camera->GetView();
    wvp[2] = camera->GetProjection();

    float3 radRotation = { transform.rotation.x * RAD, transform.rotation.y * RAD, transform.rotation.z * RAD };
    DrawMatrix::CreateWorldMatrix(wvp[0], transform.position, transform.scale, radRotation);

    ShaderList::SetWVP(wvp);
    PixelShader* pPXS = ShaderList::GetPS(ShaderList::PS_LAMBERT);

    pPXS->Bind();

    ID3D11DeviceContext* context = GetContext();

    for (int i = 0; i < a_model.GetMeshCount(); i++)
    {
        std::shared_ptr<MeshDX> mesh = a_model.GetMesh(i).lock();
        if (!mesh)
        {
            continue;
        }

        const int materialId = mesh->GetMaterialID();
        std::shared_ptr<MaterialDX> material = a_model.GetMaterial(materialId).lock();
        bool materialHasSrv = material && material->GetTexture();
        if (material && !materialHasSrv && m_warnedMissingSrvMaterials.emplace(materialId).second)
        {
            DebugMessage("[ModelRender] Material has NO SRV, fallback to white");
        }

        if (material)
        {
            if (!materialHasSrv && m_fallbackTexture)
            {
                material = std::make_shared<MaterialDX>(
                    material->GetDiffuse(),
                    material->GetAmbient(),
                    material->GetSpecular(),
                    m_fallbackTexture);
            }
            ShaderList::SetMaterial(*material);
        }
        else if (context)
        {
            ID3D11ShaderResourceView* view = m_fallbackTexture ? m_fallbackTexture->Get() : nullptr;
            context->PSSetShaderResources(0, 1, &view);
        }

        VertexShader* pVTX = ShaderList::GetVS(ShaderList::VS_WORLD);
        pVTX->Bind();

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
