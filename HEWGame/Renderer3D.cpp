#include "Renderer3D.h"

#include <Windows.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <d3dcompiler.h>
#include <sstream>

#pragma comment(lib, "d3dcompiler.lib")

namespace
{
        using namespace DirectX;

        ID3D11DeviceContext* g_context = nullptr;
        ID3D11VertexShader* g_vertexShader = nullptr;
        ID3D11PixelShader* g_pixelShader = nullptr;
        ID3D11InputLayout* g_inputLayout = nullptr;
        ID3D11Buffer* g_sceneBuffer = nullptr;
        ID3D11Buffer* g_objectBuffer = nullptr;

        struct SceneBuffer
        {
                XMMATRIX viewProjection;
                XMFLOAT3 cameraPosition;
                float padding;
        };

        struct ObjectBuffer
        {
                XMMATRIX world;
                XMFLOAT4 tint;
        };

        template <typename T>
        void SafeRelease(T*& resource)
        {
                if (resource)
                {
                        resource->Release();
                        resource = nullptr;
                }
        }

        HRESULT CompileShader(const char* source, const char* entryPoint, const char* target, ID3DBlob** blob)
        {
                UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
                compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
                ID3DBlob* errorBlob = nullptr;
                HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, entryPoint, target, compileFlags, 0, blob, &errorBlob);
                if (FAILED(hr) && errorBlob)
                {
                        std::ostringstream oss;
                        oss << "[Renderer3D] Shader compile error: " << static_cast<const char*>(errorBlob->GetBufferPointer()) << "\n";
                        OutputDebugStringA(oss.str().c_str());
                        errorBlob->Release();
                }
                return hr;
        }

        void LogIfFailed(const char* context, HRESULT hr)
        {
                if (FAILED(hr))
                {
                        std::ostringstream oss;
                        oss << "[Renderer3D] " << context << " failed. HRESULT=0x" << std::hex << hr << "\n";
                        OutputDebugStringA(oss.str().c_str());
                }
        }

        const char* VertexShaderSource()
        {
                return R"HLSL(
cbuffer SceneBuffer : register(b0)
{
        float4x4 viewProjection;
        float3 cameraPosition;
        float padding;
};
cbuffer ObjectBuffer : register(b1)
{
        float4x4 world;
        float4 tint;
};
struct VSInput
{
        float3 position : POSITION;
        float3 normal : NORMAL;
        float4 color : COLOR0;
};
struct VSOutput
{
        float4 position : SV_POSITION;
        float3 worldNormal : NORMAL;
        float3 worldPos : TEXCOORD0;
        float4 color : COLOR0;
};
VSOutput main(VSInput input)
{
        VSOutput output;
        float4 worldPos = mul(float4(input.position, 1.0f), world);
        float3 normal = mul(float4(input.normal, 0.0f), world).xyz;
        output.worldNormal = normalize(normal);
        output.worldPos = worldPos.xyz;
        output.color = input.color * tint;
        output.position = mul(worldPos, viewProjection);
        return output;
}
)HLSL";
        }

        const char* PixelShaderSource()
        {
                return R"HLSL(
cbuffer SceneBuffer : register(b0)
{
        float4x4 viewProjection;
        float3 cameraPosition;
        float padding;
};
struct PSInput
{
        float4 position : SV_POSITION;
        float3 worldNormal : NORMAL;
        float3 worldPos : TEXCOORD0;
        float4 color : COLOR0;
};
float4 main(PSInput input) : SV_TARGET
{
        float3 lightDir = normalize(float3(-0.5f, -1.0f, -0.3f));
        float diffuse = saturate(dot(input.worldNormal, -lightDir));
        float3 viewDir = normalize(cameraPosition - input.worldPos);
        float3 halfVec = normalize(viewDir - lightDir);
        float spec = pow(saturate(dot(input.worldNormal, halfVec)), 32.0f);
        float ambient = 0.35f;
        float3 finalColor = input.color.rgb * (ambient + diffuse * 0.65f) + float3(1.0f, 0.9f, 0.8f) * spec * 0.25f;
        return float4(finalColor, input.color.a);
}
)HLSL";
        }
}

namespace Renderer3D
{
        using namespace DirectX;

        bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
        {
                g_context = context;

                ID3DBlob* vsBlob = nullptr;
                HRESULT hr = CompileShader(VertexShaderSource(), "main", "vs_5_0", &vsBlob);
                if (FAILED(hr))
                {
                        LogIfFailed("Vertex shader compile", hr);
                        return false;
                }

                hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_vertexShader);
                if (FAILED(hr))
                {
                        LogIfFailed("CreateVertexShader", hr);
                        SafeRelease(vsBlob);
                        return false;
                }

                std::array<D3D11_INPUT_ELEMENT_DESC, 3> layout = {
                        D3D11_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                        D3D11_INPUT_ELEMENT_DESC{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
                        D3D11_INPUT_ELEMENT_DESC{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
                };

                hr = device->CreateInputLayout(layout.data(), static_cast<UINT>(layout.size()), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_inputLayout);
                SafeRelease(vsBlob);
                if (FAILED(hr))
                {
                        LogIfFailed("CreateInputLayout", hr);
                        return false;
                }

                ID3DBlob* psBlob = nullptr;
                hr = CompileShader(PixelShaderSource(), "main", "ps_5_0", &psBlob);
                if (FAILED(hr))
                {
                        LogIfFailed("Pixel shader compile", hr);
                        return false;
                }

                hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pixelShader);
                SafeRelease(psBlob);
                if (FAILED(hr))
                {
                        LogIfFailed("CreatePixelShader", hr);
                        return false;
                }

                D3D11_BUFFER_DESC sceneDesc = {};
                sceneDesc.ByteWidth = sizeof(SceneBuffer);
                sceneDesc.Usage = D3D11_USAGE_DEFAULT;
                sceneDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                hr = device->CreateBuffer(&sceneDesc, nullptr, &g_sceneBuffer);
                if (FAILED(hr))
                {
                        LogIfFailed("Create scene constant buffer", hr);
                        return false;
                }

                D3D11_BUFFER_DESC objectDesc = {};
                objectDesc.ByteWidth = sizeof(ObjectBuffer);
                objectDesc.Usage = D3D11_USAGE_DEFAULT;
                objectDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                hr = device->CreateBuffer(&objectDesc, nullptr, &g_objectBuffer);
                if (FAILED(hr))
                {
                        LogIfFailed("Create object constant buffer", hr);
                        return false;
                }

                OutputDebugStringA("[Renderer3D] Pipeline initialized.\n");
                return true;
        }

        void Shutdown()
        {
                SafeRelease(g_objectBuffer);
                SafeRelease(g_sceneBuffer);
                SafeRelease(g_inputLayout);
                SafeRelease(g_pixelShader);
                SafeRelease(g_vertexShader);
                g_context = nullptr;
        }

        Mesh CreateCubeMesh(ID3D11Device* device)
        {
                struct Vertex
                {
                        XMFLOAT3 position;
                        XMFLOAT3 normal;
                        XMFLOAT4 color;
                };

                const float half = 0.5f;
                const XMFLOAT4 candyPalette[] = {
                        { 1.0f, 0.6f, 0.8f, 1.0f },
                        { 0.8f, 0.9f, 0.4f, 1.0f },
                        { 0.6f, 0.8f, 1.0f, 1.0f },
                        { 1.0f, 0.8f, 0.4f, 1.0f },
                        { 0.9f, 0.5f, 1.0f, 1.0f },
                        { 0.5f, 1.0f, 0.8f, 1.0f }
                };

                std::array<Vertex, 24> vertices = {
                        Vertex{ { -half, -half, -half }, { 0.0f, 0.0f, -1.0f }, candyPalette[0] },
                        Vertex{ { -half,  half, -half }, { 0.0f, 0.0f, -1.0f }, candyPalette[0] },
                        Vertex{ {  half, -half, -half }, { 0.0f, 0.0f, -1.0f }, candyPalette[0] },
                        Vertex{ {  half,  half, -half }, { 0.0f, 0.0f, -1.0f }, candyPalette[0] },

                        Vertex{ { -half, -half,  half }, { 0.0f, 0.0f, 1.0f }, candyPalette[1] },
                        Vertex{ {  half, -half,  half }, { 0.0f, 0.0f, 1.0f }, candyPalette[1] },
                        Vertex{ { -half,  half,  half }, { 0.0f, 0.0f, 1.0f }, candyPalette[1] },
                        Vertex{ {  half,  half,  half }, { 0.0f, 0.0f, 1.0f }, candyPalette[1] },

                        Vertex{ { -half,  half, -half }, { 0.0f, 1.0f, 0.0f }, candyPalette[2] },
                        Vertex{ { -half,  half,  half }, { 0.0f, 1.0f, 0.0f }, candyPalette[2] },
                        Vertex{ {  half,  half, -half }, { 0.0f, 1.0f, 0.0f }, candyPalette[2] },
                        Vertex{ {  half,  half,  half }, { 0.0f, 1.0f, 0.0f }, candyPalette[2] },

                        Vertex{ { -half, -half, -half }, { 0.0f, -1.0f, 0.0f }, candyPalette[3] },
                        Vertex{ {  half, -half, -half }, { 0.0f, -1.0f, 0.0f }, candyPalette[3] },
                        Vertex{ { -half, -half,  half }, { 0.0f, -1.0f, 0.0f }, candyPalette[3] },
                        Vertex{ {  half, -half,  half }, { 0.0f, -1.0f, 0.0f }, candyPalette[3] },

                        Vertex{ { -half, -half,  half }, { -1.0f, 0.0f, 0.0f }, candyPalette[4] },
                        Vertex{ { -half,  half,  half }, { -1.0f, 0.0f, 0.0f }, candyPalette[4] },
                        Vertex{ { -half, -half, -half }, { -1.0f, 0.0f, 0.0f }, candyPalette[4] },
                        Vertex{ { -half,  half, -half }, { -1.0f, 0.0f, 0.0f }, candyPalette[4] },

                        Vertex{ {  half, -half, -half }, { 1.0f, 0.0f, 0.0f }, candyPalette[5] },
                        Vertex{ {  half,  half, -half }, { 1.0f, 0.0f, 0.0f }, candyPalette[5] },
                        Vertex{ {  half, -half,  half }, { 1.0f, 0.0f, 0.0f }, candyPalette[5] },
                        Vertex{ {  half,  half,  half }, { 1.0f, 0.0f, 0.0f }, candyPalette[5] }
                };

                std::array<uint16_t, 36> indices = {
                        0, 1, 2, 2, 1, 3,
                        4, 5, 6, 5, 7, 6,
                        8, 9,10,10, 9,11,
                        12,13,14,13,15,14,
                        16,17,18,18,17,19,
                        20,21,22,22,21,23
                };

                Mesh mesh;

                D3D11_BUFFER_DESC vbd = {};
                vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                vbd.ByteWidth = static_cast<UINT>(sizeof(Vertex) * vertices.size());
                vbd.Usage = D3D11_USAGE_DEFAULT;

                D3D11_SUBRESOURCE_DATA vinit = {};
                vinit.pSysMem = vertices.data();

                HRESULT hr = device->CreateBuffer(&vbd, &vinit, &mesh.vertexBuffer);
                LogIfFailed("Create cube vertex buffer", hr);
                if (FAILED(hr))
                {
                        return mesh;
                }

                D3D11_BUFFER_DESC ibd = {};
                ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
                ibd.ByteWidth = static_cast<UINT>(sizeof(uint16_t) * indices.size());
                ibd.Usage = D3D11_USAGE_DEFAULT;

                D3D11_SUBRESOURCE_DATA iinit = {};
                iinit.pSysMem = indices.data();
                hr = device->CreateBuffer(&ibd, &iinit, &mesh.indexBuffer);
                LogIfFailed("Create cube index buffer", hr);
                if (FAILED(hr))
                {
                        SafeRelease(mesh.vertexBuffer);
                        return mesh;
                }

                mesh.indexCount = static_cast<UINT>(indices.size());

                OutputDebugStringA("[Renderer3D] Candy cube mesh created.\n");
                return mesh;
        }

        void ReleaseMesh(Mesh& mesh)
        {
                SafeRelease(mesh.vertexBuffer);
                SafeRelease(mesh.indexBuffer);
                mesh.indexCount = 0;
        }

        void UpdateScene(const XMMATRIX& view, const XMMATRIX& projection, const XMFLOAT3& cameraPosition)
        {
                if (!g_context)
                {
                        return;
                }

                SceneBuffer buffer;
                buffer.viewProjection = XMMatrixTranspose(view * projection);
                buffer.cameraPosition = cameraPosition;
                buffer.padding = 0.0f;
                g_context->UpdateSubresource(g_sceneBuffer, 0, nullptr, &buffer, 0, 0);
        }

        void DrawMesh(const Mesh& mesh, const XMMATRIX& world, const XMFLOAT4& tintColor)
        {
                if (!mesh.vertexBuffer || !mesh.indexBuffer || mesh.indexCount == 0)
                {
                        return;
                }

                UINT stride = 32;
                UINT offset = 0;
                g_context->IASetInputLayout(g_inputLayout);
                g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                g_context->IASetVertexBuffers(0, 1, &mesh.vertexBuffer, &stride, &offset);
                g_context->IASetIndexBuffer(mesh.indexBuffer, DXGI_FORMAT_R16_UINT, 0);

                g_context->VSSetShader(g_vertexShader, nullptr, 0);
                g_context->PSSetShader(g_pixelShader, nullptr, 0);
                g_context->VSSetConstantBuffers(0, 1, &g_sceneBuffer);
                g_context->PSSetConstantBuffers(0, 1, &g_sceneBuffer);

                ObjectBuffer objectData;
                objectData.world = XMMatrixTranspose(world);
                objectData.tint = tintColor;
                g_context->UpdateSubresource(g_objectBuffer, 0, nullptr, &objectData, 0, 0);

                g_context->VSSetConstantBuffers(1, 1, &g_objectBuffer);
                g_context->PSSetConstantBuffers(1, 1, &g_objectBuffer);

                g_context->DrawIndexed(mesh.indexCount, 0, 0);
        }
}
