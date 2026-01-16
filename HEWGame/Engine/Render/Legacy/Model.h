/*
* @brief 
* @ 合成アニメーションとは
*/
#ifndef __MODEL_H__
#define __MODEL_H__

#include <DirectXMath.h>
#include <vector>
#include "Shader.h"
#include "MeshBuffer.h"
#include <functional>

class Model
{
public:
	// モデル反転設定
	enum Flip
	{
		None,			// DirectX準拠(反転する
		XFlip,			// Maya準拠
		ZFlip,			// DirectX準拠(Mayaから180回転した状態
	};

public:
	// 型定義
	using NodeIndex	= int;	// ボーン(階層)番号

	// 定数定義
	static const NodeIndex	INDEX_NONE = -1;		// 該当ノードなし

private:
	// 内部型定義
	using Children	= std::vector<NodeIndex>;	// ノード階層情報

	// 内部定数定義
	static const UINT		MAX_BONE			=	200;	// １メッシュの最大ボーン数(ここを変更する場合.hlsl側の定義も変更する

	// ボーン情報
	struct Node
	{
		std::string			name;		// ボーン名
		NodeIndex			parent;		// 親ボーン
		Children			children;	// 子ボーン
		DirectX::XMMATRIX	mat;		// 変換行列
	};
	using Nodes = std::vector<Node>;

	
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
	using Vertices	= std::vector<Vertex>;
	using Indices	= std::vector<unsigned long>;

	// 頂点の骨変形情報
	struct Bone
	{
		NodeIndex index;
		DirectX::XMMATRIX invOffset;
	};
	using Bones = std::vector<Bone>;

	// メッシュ
	struct Mesh
	{
		Vertices		vertices;
		Indices			indices;
		unsigned int	materialID;
		Bones			bones;
		MeshBuffer*		pMesh;
	};
	using Meshes = std::vector<Mesh>;

	// マテリアル情報
	struct Material
	{
		DirectX::XMFLOAT4	diffuse;	// 拡散光(メインカラー
		DirectX::XMFLOAT4	ambient;	// 環境光(陰の部分のカラー
		DirectX::XMFLOAT4	specular;	// 鏡面反射光(強く光る部分のカラー
		Texture* pTexture;	// テクスチャ
	};
	using Materials = std::vector<Material>;

public:
	Model();
	~Model();
	void Reset();
	void SetVertexShader(VertexShader* vs);
	void SetPixelShader(PixelShader* ps);
	bool Load(const char* file, float scale = 1.0f, Flip flip = Flip::None);
	void Draw(int meshNo = -1);

	//--- 各種情報取得
	const Mesh* GetMesh(unsigned int index);
	uint32_t GetMeshNum();
	const Material* GetMaterial(unsigned int index);
	uint32_t GetMaterialNum();
	DirectX::XMMATRIX GetBone(NodeIndex index);



private:
	static Texture*		m_pWhiteTex;	// ftHgeNX`
	static unsigned int		m_textureRef;	// ftHgeNX`QƐ
	// 各種生成
	void MakeMesh(const void* ptr, float scale, Flip flip);
	void MakeMaterial(const void* ptr, std::string directory);
	void MakeBoneNodes(const void* ptr);
	void MakeWeight(const void* ptr, int meshIdx);

	// 内部計算

private:
	static VertexShader*	m_pDefVS;		// デフォルト頂点シェーダー
	static PixelShader*		m_pDefPS;		// デフォルトピクセルシェーダー
	static unsigned int		m_shaderRef;	// シェーダー参照数
#ifdef _DEBUG
	static std::string m_errorStr;	
#endif

private:
	float			m_loadScale;	// 
	Flip			m_loadFlip;		// 

	Meshes			m_meshes;		// メッシュ配列
	Materials		m_materials;	// マテリアル配列
	Nodes			m_nodes;		// 階層情報
	VertexShader*	m_pVS;			// 設定中の頂点シェーダ
	PixelShader*	m_pPS;			// 設定中のピクセルシェーダ


	// 以下越智による変更点
	
};


#endif
