#include "ModelRender.h"

#include "DrawMatrix.h"
#include "ShaderList.h"
#include "Defines.h"
#include "Vector.h"
#include "DirectX.h"

#include <sstream>
#include <string>

#if defined(_WIN32)
#       ifndef NOMINMAX
#               define NOMINMAX
#       endif
#       include <windows.h>
#endif

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
        void DebugTrace(const std::string& message)
        {
                ::OutputDebugStringA(message.c_str());
        }
#else
        void DebugTrace(const std::string& message)
        {
                (void)message;
        }
#endif

        constexpr const char* kLogPrefix = "[ModelRender] ";
}

ModelRender::ModelRender() = default;

ModelRender::~ModelRender() = default;

void ModelRender::Draw(ModelDX& a_model, Transform transform, const CameraSpace::ICamera* camera)
{
    if (!camera)
    {
        DebugTrace(std::string(kLogPrefix) + "Draw aborted: camera pointer was null.\n");
        return;
    }

    DirectX::XMFLOAT4X4 wvp[3];
    wvp[1] = camera->GetView();
    wvp[2] = camera->GetProjection();

    float3 radRotation = { transform.rotation.x * RAD, transform.rotation.y * RAD, transform.rotation.z * RAD };
    DrawMatrix::CreateWorldMatrix(wvp[0], transform.position, transform.scale, radRotation);

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
            DebugTrace(std::string(kLogPrefix) + "Skipped mesh draw: mesh resource expired.\n");
            continue;
        }

        if (i < a_model.GetMeshCount())
        {
            std::shared_ptr<MaterialDX> material = a_model.GetMaterial(mesh->GetMaterialID()).lock();
            if (material)
            {
                ShaderList::SetMaterial(*material);
            }
            else
            {
                DebugTrace(std::string(kLogPrefix) + "Material missing for mesh, default material used.\n");
                if (context)
                {
                    ID3D11ShaderResourceView* nullView = nullptr;
                    context->PSSetShaderResources(0, 1, &nullView);
                }
            }
        }

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
