#include "ModelRender.h"
#include "ModelDX.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "Animation.h"
#include "DrawMatrix.h"
#include "ShaderList.h"
#include "Defines.h"
#include "Vector.h"
#include "DirectX.h"

namespace
{
    bool IsVisible(const Render::Model& model, const DirectX::XMFLOAT4X4 (&wvp)[3])
    {
        if (!model.HasLocalBounds()) return true;
        const AABB& bounds = model.GetLocalBounds();
        const float values[] = {bounds.min.x, bounds.min.y, bounds.min.z,
            bounds.max.x, bounds.max.y, bounds.max.z};
        for (float value : values) if (!std::isfinite(value)) return true;
        if (bounds.min.x > bounds.max.x || bounds.min.y > bounds.max.y || bounds.min.z > bounds.max.z)
            return true;

        // ミカ：描画モデルの八隅を同じ行列で判定し、境界と不明な範囲は描画側へ残す
        const auto clipMatrix = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&wvp[0]))
            * DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&wvp[1]))
            * DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&wvp[2]));
        unsigned outside = 0x3f;
        for (unsigned corner = 0; corner < 8; ++corner)
        {
            DirectX::XMFLOAT4 clip;
            DirectX::XMStoreFloat4(&clip, DirectX::XMVector4Transform(DirectX::XMVectorSet(
                corner & 1 ? bounds.max.x : bounds.min.x,
                corner & 2 ? bounds.max.y : bounds.min.y,
                corner & 4 ? bounds.max.z : bounds.min.z, 1.0f), clipMatrix));
            if (!std::isfinite(clip.x) || !std::isfinite(clip.y)
                || !std::isfinite(clip.z) || !std::isfinite(clip.w)) return true;
            const float margin = 1.0e-5f * std::max({1.0f, std::fabs(clip.x),
                std::fabs(clip.y), std::fabs(clip.z), std::fabs(clip.w)});
            const unsigned planes = (clip.x < -clip.w - margin ? 1u : 0u)
                | (clip.x > clip.w + margin ? 2u : 0u)
                | (clip.y < -clip.w - margin ? 4u : 0u)
                | (clip.y > clip.w + margin ? 8u : 0u)
                | (clip.z < -margin ? 16u : 0u)
                | (clip.z > clip.w + margin ? 32u : 0u);
            outside &= planes;
            if (outside == 0) return true;
        }
        return outside == 0;
    }
}

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

    // ミカ：アニメーションの可動範囲は静止時の境界では除外しない
    if (!animation && !IsVisible(a_model, wvp)) return;

    VertexShader* pVTX = ShaderList::GetVS(ShaderList::VS_WORLD);
    PixelShader* pPXS = ShaderList::GetPS(ShaderList::PS_LAMBERT);

    if (animation) ShaderList::SetWVP(wvp);
    else pVTX->WriteBuffer(0, wvp);

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
        const unsigned int boneCount = mesh->GetBoneCount();
        const size_t limitedBoneCount = std::min<size_t>(boneCount, kMaxBones);

        const bool useAnimationShader = (boneCount > 0) && (animation != nullptr);

        if (useAnimationShader)
        {
            // ミカ：静止メッシュでは不要な二百本分のボーン作成と転送を省く
            std::array<DirectX::XMFLOAT4X4, kMaxBones> newBones{};
            for (size_t boneIdx = 0; boneIdx < kMaxBones; ++boneIdx)
            {
                DirectX::XMStoreFloat4x4(&newBones[boneIdx], DirectX::XMMatrixIdentity());
            }
            pVTX = ShaderList::GetVS(ShaderList::VS_ANIME);
            pVTX->Bind();
            for (size_t boneIdx = 0; boneIdx < limitedBoneCount; ++boneIdx)
            {
                DirectX::XMStoreFloat4x4(
                    &newBones[boneIdx],
                    DirectX::XMMatrixTranspose(mesh->GetBoneMatrix(boneIdx) *
                        animation->GetNodeMatrix(mesh->GetBoneNodeIndex(boneIdx))));
            }
            ShaderList::SetBones(newBones.data());
        }
        else if (pVTX != ShaderList::GetVS(ShaderList::VS_WORLD))
        {
            pVTX = ShaderList::GetVS(ShaderList::VS_WORLD);
            pVTX->Bind();
        }


        if (material)
        {
            ShaderList::SetMaterial(*material, animation ? nullptr : pPXS);

            if (context)
            {
                ID3D11ShaderResourceView* srv = material->GetTexture();
                context->PSSetShaderResources(0, 1, &srv);
            }
        }
        else
        {
            if (context)
            {
                ID3D11ShaderResourceView* nullView = nullptr;
                context->PSSetShaderResources(0, 1, &nullView);
            }
        }

        MeshDraw(*mesh);
    }

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
