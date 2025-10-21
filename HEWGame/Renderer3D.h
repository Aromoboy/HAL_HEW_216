#pragma once

#include <d3d11.h>
#include <DirectXMath.h>

namespace Renderer3D
{
        struct Mesh
        {
                ID3D11Buffer* vertexBuffer = nullptr;
                ID3D11Buffer* indexBuffer = nullptr;
                UINT indexCount = 0;
        };

        bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
        void Shutdown();

        Mesh CreateCubeMesh(ID3D11Device* device);
        void ReleaseMesh(Mesh& mesh);

        void UpdateScene(const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& projection, const DirectX::XMFLOAT3& cameraPosition);
        void DrawMesh(const Mesh& mesh, const DirectX::XMMATRIX& world, const DirectX::XMFLOAT4& tintColor);
}
