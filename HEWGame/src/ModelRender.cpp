#include "ModelRender.h"

#include <algorithm>
#include <array>

#include "Animation.h"
#include "DrawMatrix.h"
#include "ShaderList.h"
#include "Defines.h"
#include "Vector.h"
#include "DirectX.h"

ModelRender::ModelRender() = default;
ModelRender::~ModelRender() = default;

void ModelRender::Draw(Render::Model& a_model, Transform transform, const CameraSpace::ICamera* camera, Animation* animation)
{
    if (!camera)
    {
        return;
    }

    DirectX::XMFLOAT4X4 wvp[3];
    wvp[1] = camera->GetView();
    wvp[2] = camera->GetProjection();

    
    DrawMatrix::CreateWorldMatrix(
        wvp[0],
        transform.position,
        transform.scale,
        transform.rotation
    );

    ShaderList::SetWVP(wvp);

    // 一旦すべてAnimeで描画
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



        constexpr size_t kMaxBones = 200;
        std::array<DirectX::XMFLOAT4X4, kMaxBones> newBones;
        const unsigned int boneCount = mesh->GetBoneCount();
        const size_t limitedBoneCount = std::min<size_t>(boneCount, kMaxBones);
        DirectX::XMFLOAT4X4 identityMatrix;
        DirectX::XMStoreFloat4x4(&identityMatrix, DirectX::XMMatrixIdentity());
        newBones.fill(identityMatrix);

        const bool useAnimationShader = (limitedBoneCount > 0) && (animation != nullptr);
        //if (useAnimationShader)
        //{
        //    for (size_t i = 0; i < limitedBoneCount; ++i)
        //    {
        //        const NodeIndexDX nodeIndex = a_model.GetBoneNodeIndex(static_cast<unsigned int>(i));
        //       DirectX::XMStoreFloat4x4(&bones[i], a_model.GetBoneMatrix(i) * animation->GetNodeMatrix(nodeIndex));
        //    }
        //}
        if (useAnimationShader)
        {
            pVTX = ShaderList::GetVS(ShaderList::VS_ANIME);
            pVTX->Bind();
            for (size_t boneIdx = 0; boneIdx < limitedBoneCount; ++boneIdx)
            {
                DirectX::XMStoreFloat4x4(
                    &newBones[boneIdx],
                    DirectX::XMMatrixTranspose(mesh->GetBoneMatrix(boneIdx) *
                        animation->GetNodeMatrix(mesh->GetBoneNodeIndex(boneIdx))));
            }
        }

        ShaderList::SetBones(newBones.data());


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
