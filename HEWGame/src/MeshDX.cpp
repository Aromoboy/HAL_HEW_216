// ============================================================================
// Tv: MeshDX ̋֘AWbNAsrc ɑ@\̉ĂB
// ړI: ߂ẴW[ɐGJ҂łAӖƈˑ֌Wfcł悤ɍx̐t^ĂB
// Ag: src z̑t@CVXewƘA邽߁At@CԂ̊֌WǐՂۂ̖ڈƂėpłB
// ============================================================================

#include "MeshDX.h"

#include "Debug.h"

MeshDX::MeshDX(const MeshDX::Description& desc, unsigned long materialID)
	: m_pVtxBuffer(nullptr)
	, m_pIdxBuffer(nullptr)
{
	HRESULT hr;

	// 各種ステータスを設定
	m_indexCount = desc.idxCount;

	m_materialID = materialID;

	// 頂点バッファ作成
	hr = CreateVertexBuffer(desc.pVtx, desc.vtxSize, desc.vtxCount);
	if (FAILED(hr))
	{
#ifdef _DEBUG
		DebugMessage("頂点バッファの作成に失敗しました。");
#endif
		return;
	}
	// インデックスバッファ作成
	if (desc.pIdx) {
		hr = CreateIndexBuffer(desc.pIdx, desc.idxSize, desc.idxCount);
		if (FAILED(hr))
		{
#ifdef _DEBUG
			DebugMessage("インデックスバッファの作成に失敗しました。");
#endif


			return;
		}
	}

	

}

MeshDX::~MeshDX()
{
	if (m_pVtxBuffer) m_pVtxBuffer->Release();
	if (m_pIdxBuffer) m_pIdxBuffer->Release();
}


// 各種ゲッタ
unsigned int MeshDX::GetMaterialID()
{
	return m_materialID;
}

ID3D11Buffer* MeshDX::GetVertecBuffer()
{
	return m_pVtxBuffer;
}

ID3D11Buffer* MeshDX::GetIndexBuffer()
{
	return m_pIdxBuffer;
}

unsigned int MeshDX::GetIndexCount()
{
	return m_indexCount;
}


// 頂点バッファの作成
HRESULT MeshDX::CreateVertexBuffer(const void* pVtx, UINT size, UINT count)
{
	//--- 作成するバッファの情報
	D3D11_BUFFER_DESC bufDesc = {};
	bufDesc.ByteWidth = size * count;
	bufDesc.Usage = D3D11_USAGE_DEFAULT;
	bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	//--- バッファの初期値を設定
	D3D11_SUBRESOURCE_DATA subResource = {};
	subResource.pSysMem = pVtx;

	//--- 頂点バッファの作成
	HRESULT hr;
	ID3D11Device* pDevice = GetDevice();
	hr = pDevice->CreateBuffer(&bufDesc, &subResource, &m_pVtxBuffer);

	return hr;
}

// 頂点バッファの作成
HRESULT MeshDX::CreateIndexBuffer(const void* pIdx, UINT size, UINT count)
{
	// インデックスサイズの確認
	switch (size)
	{
	default:
		return E_FAIL;
	case 2:
	case 4:
		break;
	}

	// バッファの情報を設定
	D3D11_BUFFER_DESC bufDesc = {};
	bufDesc.ByteWidth = size * count;
	bufDesc.Usage = D3D11_USAGE_DEFAULT;
	bufDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	// バッファの初期データ
	D3D11_SUBRESOURCE_DATA subResource = {};
	subResource.pSysMem = pIdx;

	// インデックスバッファ生成
	ID3D11Device* pDevice = GetDevice();
	HRESULT hr;
	hr = pDevice->CreateBuffer(&bufDesc, &subResource, &m_pIdxBuffer);

	return hr;
}
