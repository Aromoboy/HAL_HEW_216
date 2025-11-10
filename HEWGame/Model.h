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
		ZFlipUseAnime,	// DirecX準拠(アニメーションさせる場合
	};

private:
	// アニメーション計算領域
	enum AnimeTransform
	{
		MAIN,			// 通常再生
		BLEND,			// ブレンド再生
		PARAMETRIC0,	// 合成A
		PARAMETRIC1,	// 合成B
		MAX_TRANSFORM
	};

public:
	// 型定義
	using NodeIndex	= int;	// ボーン(階層)番号
	using AnimeNo	= int;	// アニメーション番号

	// 定数定義
	static const NodeIndex	INDEX_NONE = -1;		// 該当ノードなし
	static const AnimeNo	ANIME_NONE = -1;		// 該当アニメーションなし
	static const AnimeNo	PARAMETRIC_ANIME = -2;	// 合成アニメーション

private:
	// 内部型定義
	using Children	= std::vector<NodeIndex>;	// ノード階層情報

	// 内部定数定義
	static const UINT		MAX_BONE			=	200;	// １メッシュの最大ボーン数(ここを変更する場合.hlsl側の定義も変更する

	// アニメーションの変換情報
	struct Transform
	{
		DirectX::XMFLOAT3	translate;
		DirectX::XMFLOAT4	quaternion;
		DirectX::XMFLOAT3	scale;
	};
	using Key			= std::pair<float, Transform>;
	using Timeline		= std::map<float, Transform>;
	using Transforms	= std::vector<Transform>;

	// アニメーションとボーンの関連付け情報
	struct Channel
	{
		NodeIndex	index;
		Timeline	timeline;
	};
	using Channels = std::vector<Channel>;

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

	// アニメーション情報
	struct Animation
	{
		float		nowTime;	// 現在の再生時間
		float		totalTime;	// 最大再生時間
		float		speed;		// 再生速度
		bool		isLoop;		// ループ指定
		Channels	channels;	// 変換情報
	};
	using Animations = std::vector<Animation>;

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
	const Animation* GetAnimation(AnimeNo no);

	//--- アニメーション
	// アニメーションの読み込み
	AnimeNo AddAnimation(const char* file);
	// アニメーションの更新
	void Step(float tick);

	// アニメーションの再生
	void Play(AnimeNo no, bool loop, float speed = 1.0f);
	// アニメーションのブレンド再生
	void PlayBlend(AnimeNo no, float blendTime, bool loop, float speed = 1.0f);
	// アニメーションの合成設定
	void SetParametric(AnimeNo no1, AnimeNo no2);
	// アニメーションの合成割合設定
	void SetParametricBlend(float blendRate);
	// アニメーションの現在再生時間を変更
	void SetAnimationTime(AnimeNo no, float time);

	// 再生フラグ
	bool IsPlay(AnimeNo no);
	// 現在再生中のアニメ番号
	AnimeNo GetPlayNo();
	// ブレンド中のアニメ番号
	AnimeNo GetBlendNo();

#ifdef _DEBUG
	static std::string GetError();
	void DrawBone();
#endif


private:
	// 各種生成
	void MakeMesh(const void* ptr, float scale, Flip flip);
	void MakeMaterial(const void* ptr, std::string directory);
	void MakeBoneNodes(const void* ptr);
	void MakeWeight(const void* ptr, int meshIdx);

	// 内部計算
	bool AnimeNoCheck(AnimeNo no);
	void InitAnime(AnimeNo no);
	void CalcAnime(AnimeTransform kind, AnimeNo no);
	void UpdateAnime(AnimeNo no, float tick);
	void CalcBones(NodeIndex node, const DirectX::XMMATRIX parent);
	void LerpTransform(Transform* pOut, const Transform& a, const Transform& b, float rate);

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
	Animations		m_animes;		// アニメ配列
	VertexShader*	m_pVS;			// 設定中の頂点シェーダ
	PixelShader*	m_pPS;			// 設定中のピクセルシェーダ
	
	AnimeNo			m_playNo;			// 現在再生中のアニメ番号
	AnimeNo			m_blendNo;			// ブレンド再生を行うアニメ番号
	AnimeNo			m_parametric[2];	// 合成再生を行うアニメ番号
	float			m_blendTime;		// 現在の遷移経過時間
	float			m_blendTotalTime;	// アニメ遷移にかかる合計時間
	float			m_parametricBlend;	// パラメトリックの再生割合

	Transforms		m_nodeTransform[MAX_TRANSFORM];	// アニメーション別変形情報


	// 以下越智による変更点
	
};


#endif // __MODEL_H__