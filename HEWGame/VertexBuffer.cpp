#include "VertexBuffer.h"

ID3D11Buffer* g_pUnitVertexBuffer = nullptr;
#define SPRITE_VERTEX (4)


ID3D11Buffer* CreateVertexBuffer(ID3D11Device* device, Vertex* vtxData, UINT vtxNum)
{
        if (!device)
        {
                return nullptr;
        }

        D3D11_BUFFER_DESC vtxBufDesc;
        ZeroMemory(&vtxBufDesc, sizeof(vtxBufDesc));
        vtxBufDesc.ByteWidth = sizeof(Vertex) * vtxNum; // obt@̑傫
        vtxBufDesc.Usage = D3D11_USAGE_DEFAULT; // ł̊Ǘ@
        vtxBufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER; // GPUł̗p@

        // obt@f[^@ݒ
        D3D11_SUBRESOURCE_DATA vtxSubResource;
        ZeroMemory(&vtxSubResource, sizeof(vtxSubResource));
        vtxSubResource.pSysMem = vtxData; // obt@ɗރf[^


        // 쐬
        HRESULT hr;
        ID3D11Buffer *pVtxBuf;
        hr = device->CreateBuffer(&vtxBufDesc, &vtxSubResource, &pVtxBuf);
        if (FAILED(hr)) { return nullptr; }
        return pVtxBuf;
}

ID3D11Buffer* GetUnitVertexBuffer()
{
        return g_pUnitVertexBuffer;
}

void InitVertex(ID3D11Device* device)
{
        Vertex vtx[] = {
        {{-0.5f,-0.5f,0.0f,}, {0.0f,0.0f}},
        {{-0.5f, 0.5f,0.0f,}, {0.0f,1.0f}},
        {{ 0.5f,-0.5f,0.0f,}, {1.0f,0.0f}},
        {{ 0.5f, 0.5f,0.0f,}, {1.0f,1.0f}},
        };


        g_pUnitVertexBuffer = CreateVertexBuffer(device, vtx, SPRITE_VERTEX);
}

void UninitVertex()
{
        if (g_pUnitVertexBuffer)
        {
                g_pUnitVertexBuffer->Release();
                g_pUnitVertexBuffer = nullptr;
        }
}

