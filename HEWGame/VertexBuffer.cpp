#include "VertexBuffer.h"

ID3D11Buffer* g_pUnitVertexBuffer;
#define SPRITE_VERTEX (4)


ID3D11Buffer* CreateVertexBuffer(Vertex* vtxData, UINT vtxNum)
{

	D3D11_BUFFER_DESC vtxBufDesc;
	ZeroMemory(&vtxBufDesc, sizeof(vtxBufDesc));
	vtxBufDesc.ByteWidth = sizeof(Vertex) * vtxNum; // バッファの大きさ
	vtxBufDesc.Usage = D3D11_USAGE_DEFAULT; // メモリ上での管理方法
	vtxBufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER; // GPU上での利用方法

	// バッファ初期データ　設定
	D3D11_SUBRESOURCE_DATA vtxSubResource;
	ZeroMemory(&vtxSubResource, sizeof(vtxSubResource));
	vtxSubResource.pSysMem = vtxData; // バッファに流し込むデータ


	// 作成
	HRESULT hr;
	ID3D11Buffer *pVtxBuf;
	hr = GetDevice()->CreateBuffer(&vtxBufDesc, &vtxSubResource, &pVtxBuf);
	if (FAILED(hr)) { return nullptr; }
	return pVtxBuf;
}

ID3D11Buffer* GetUnitVertexBuffer()
{
	return g_pUnitVertexBuffer;
}

void InitVertex()
{
	Vertex vtx[] = {
	{{-0.5f,-0.5f,0.0f,}, {0.0f,0.0f}},
	{{-0.5f, 0.5f,0.0f,}, {0.0f,1.0f}},
	{{ 0.5f,-0.5f,0.0f,}, {1.0f,0.0f}},
	{{ 0.5f, 0.5f,0.0f,}, {1.0f,1.0f}},
	};


	g_pUnitVertexBuffer = CreateVertexBuffer(vtx, SPRITE_VERTEX);
}

void UninitVertex()
{
	if (g_pUnitVertexBuffer) g_pUnitVertexBuffer->Release();
}
