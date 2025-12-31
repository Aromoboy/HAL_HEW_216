// 描画基盤向けのメッシュ情報を保持する
// モデル描画やローダーから参照される

#pragma once

#include "DirectX.h"
#include "Core/VertexBuffer.h"
#include "Core/IndexBuffer.h"
#include <DirectXMath.h>
#include <vector>
#include <string>


using NodeIndexDX = int;	// �{�[��(�K�w)�ԍ�
using ChildrenDX = std::vector<NodeIndexDX>;

struct BoneDX
{
	NodeIndexDX			nodeIndex;	// �ό`���͊K�w���Ɋi�[
	DirectX::XMFLOAT4X4	invOffset;	// �t�s��
};



// �{�[�����
struct NodeDX
{
	std::string			name;		// �{�[����
	NodeIndexDX			parent;		// �e�{�[��
	ChildrenDX			children;	// �q�{�[��
	DirectX::XMMATRIX	mat;		// �ϊ��s��
};



constexpr float kNodeNone = -1.0f;

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
		std::vector<BoneDX> bones;
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


	unsigned int GetBoneCount() const;

	DirectX::XMMATRIX GetBoneMatrix(unsigned int index);
	NodeIndexDX GetBoneNodeIndex(unsigned int index) const;

private:

	// 各バッファ
        Core::VertexBuffer m_vertexBuffer;
        Core::IndexBuffer m_indexBuffer;
        bool m_hasIndexBuffer;

	// 対応するマテリアルID
	unsigned int m_materialID;

	// Index数
	unsigned int m_indexCount;

	// Bone
	std::vector<BoneDX> m_bones;

	// コンストラクタで使用するバッファ作成処理
        HRESULT CreateVertexBuffer(const void* pVtx, UINT size, UINT count);
        HRESULT CreateIndexBuffer(const void* pIdx, UINT size, UINT count);
};

