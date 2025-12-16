#include "ModelRender.h"

#include "DrawMatrix.h"
#include "ShaderList.h"
#include "Defines.h"
#include "Vector.h"
#include "DirectX.h"

ModelRender::ModelRender() = default;
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

    float3 radRotation = {
        transform.rotation.x * RAD,
        transform.rotation.y * RAD,
        transform.rotation.z * RAD
    };

    DrawMatrix::CreateWorldMatrix(
        wvp[0],
        transform.position,
        transform.scale,
        radRotation
    );

    ShaderList::SetWVP(wvp);

    VertexShader* pVTX = ShaderList::GetVS(ShaderList::VS_WORLD);
    PixelShader* pPXS = ShaderList::GetPS(ShaderList::PS_LAMBERT);

    pVTX->Bind();
    pPXS->Bind();

    ID3D11DeviceContext* context = GetContext();

    for (int i = 0; i < a_model.GetMeshCount(); i++)
    {
        std::shared_ptr<MeshDX> mesh = a_model.GetMesh(i).lock();
        if (!mesh)
        {
            continue;
        }

        std::shared_ptr<MaterialDX> material =
            a_model.GetMaterial(mesh->GetMaterialID()).lock();

        if (material)
        {
            // 1️⃣ Константы материала
            ShaderList::SetMaterial(*material);

            // 2️⃣ БИНД ТЕКСТУРЫ В PS (ЭТОГО НЕ ХВАТАЛО)
            if (context)
            {
                ID3D11ShaderResourceView* srv = material->GetTexture();
                context->PSSetShaderResources(0, 1, &srv);
            }
        }
        else
        {
            // 3️⃣ Если материала нет — чистим слот
            if (context)
            {
                ID3D11ShaderResourceView* nullView = nullptr;
                context->PSSetShaderResources(0, 1, &nullView);
            }
        }

        MeshDraw(*mesh);
    }

    // 4️⃣ После отрисовки — подчистить слот (DX любит порядок)
    if (context)
    {
        ID3D11ShaderResourceView* nullView = nullptr;
        context->PSSetShaderResources(0, 1, &nullView);
    }
}

void ModelRender::MeshDraw(MeshDX& a_mesh)
{
    ID3D11DeviceContext* pContext = GetContext();
    UINT offset = 0;

    const Core::VertexBuffer& vertexBuffer = a_mesh.GetVertexBuffer();
    vertexBuffer.Bind(0, offset);

    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

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
