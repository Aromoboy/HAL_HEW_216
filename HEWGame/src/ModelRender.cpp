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
                if (context)
                {
                    material->BindTexture(context);
                }
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
        UINT stride = sizeof(MeshDX::Vertex);
        UINT offset = 0;

        ID3D11Buffer* vertex = a_mesh.GetVertecBuffer();
        ID3D11Buffer* index = a_mesh.GetIndexBuffer();

        pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pContext->IASetVertexBuffers(0, 1, &vertex, &stride, &offset);

        DXGI_FORMAT format = DXGI_FORMAT_R32_UINT;
        if (sizeof(unsigned long) == 2)
        {
                format = DXGI_FORMAT_R16_UINT;
        }
        pContext->IASetIndexBuffer(index, format, 0);
        pContext->DrawIndexed(a_mesh.GetIndexCount(), 0, 0);
}
