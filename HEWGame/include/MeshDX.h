#pragma once

#include "DirectX.h"
#include "Core/VertexBuffer.h"
#include "Core/IndexBuffer.h"
#include <DirectXMath.h>

class MeshDX
{
public:
	// 頂点情報
	struct Vertex
	{
		DirectX::XMFLOAT3	pos;
		DirectX::XMFLOAT3	normal;
		DirectX::XMFLOAT2	uv;
		DirectX::XMFLOAT4	color;
		float				weight[4];
		unsigned int		index[4];
	};

	struct PackedVertex
	{
		DirectX::XMFLOAT3 pos;
		DirectX::XMFLOAT3 normal;
		DirectX::XMFLOAT2 uv;
		DirectX::XMFLOAT4 color;
	};

	// 作成用の情報
	struct Description
	{
		const void* pVtx;
		UINT vtxSize;
		UINT vtxCount;
		const void* pIdx;
		UINT idxSize;
		UINT idxCount;
		D3D11_PRIMITIVE_TOPOLOGY topology;
	};

	// コンストラクタ、デストラクタ
	MeshDX(const MeshDX::Description& desc, unsigned long materialID);
	~MeshDX();
	
	// メッシュに対応するモデルのマテリアルIDを取得
	unsigned int GetMaterialID();

	// 各バッファの取得
        const Core::VertexBuffer& GetVertexBuffer() const;
        const Core::IndexBuffer* GetIndexBuffer() const;
        bool HasIndexBuffer() const;

	// Index数の取得
	unsigned int GetIndexCount();

private:

	// 各バッファ
        Core::VertexBuffer m_vertexBuffer;
        Core::IndexBuffer m_indexBuffer;
        bool m_hasIndexBuffer;

	// 対応するマテリアルID
	unsigned int m_materialID;

	// Index数
	unsigned int m_indexCount;

	// コンストラクタで使用するバッファ作成処理
        HRESULT CreateVertexBuffer(const void* pVtx, UINT size, UINT count);
        HRESULT CreateIndexBuffer(const void* pIdx, UINT size, UINT count);
};

