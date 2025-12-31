#include "Model.h"
#include "TextureLoad.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#ifdef _DEBUG
#include "Geometory.h"
#include "Debug.h"
#include <string>
#include "math.h"
#endif

#if _MSC_VER >= 1930
#ifdef _DEBUG
#pragma comment(lib, "assimp-vc143-mtd.lib")
#else
#pragma comment(lib, "assimp-vc143-mt.lib")
#endif
#elif _MSC_VER >= 1920
#ifdef _DEBUG
#pragma comment(lib, "assimp-vc142-mtd.lib")
#else
#pragma comment(lib, "assimp-vc142-mt.lib")
#endif
#elif _MSC_VER >= 1910
#ifdef _DEBUG
#pragma comment(lib, "assimp-vc141-mtd.lib")
#else
#pragma comment(lib, "assimp-vc141-mt.lib")
#endif
#endif

// staticメンバ変数定義
VertexShader*	Model::m_pDefVS		= nullptr;
PixelShader*	Model::m_pDefPS		= nullptr;
unsigned int	Model::m_shaderRef	= 0;
Texture*	Model::m_pWhiteTex	= nullptr;
unsigned int	Model::m_textureRef	= 0;
#ifdef _DEBUG
std::string		Model::m_errorStr	= "";
#endif

/*
* @brief assimp内の行列をXMMATRIX型に変換
* @param[in] M assimpの行列
* @return 変換後の行列
*/
DirectX::XMMATRIX GetMatrixFromAssimpMatrix(aiMatrix4x4 M)
{
	return DirectX::XMMatrixSet(
		M.a1, M.b1, M.c1, M.d1,
		M.a2, M.b2, M.c2, M.d2,
		M.a3, M.b3, M.c3, M.d3,
		M.a4, M.b4, M.c4, M.d4
	);
}

/*
* @brief デフォルトのシェーダーを作成
* @param[out] vs 頂点シェーダー格納先
* @param[out] ps ピクセルシェーダー格納先
*/
void MakeModelDefaultShader(VertexShader** vs, PixelShader** ps)
{
	const char* ModelVS = R"EOT(
struct VS_IN {
	float3 pos : POSITION0;
	float3 normal : NORMAL0;
	float2 uv : TEXCOORD0;
};
struct VS_OUT {
	float4 pos : SV_POSITION;
	float3 normal : NORMAL0;
	float2 uv : TEXCOORD0;
};
VS_OUT main(VS_IN vin) {
	VS_OUT vout;
	vout.pos = float4(vin.pos, 1.0f);
	vout.pos.z += 0.5f;
	vout.pos.y -= 0.8f;
	vout.normal = vin.normal;
	vout.uv = vin.uv;
	return vout;
})EOT";
        const char* ModelPS = R"EOT(
struct PS_IN {
	float4 pos : SV_POSITION;
	float3 normal : NORMAL0;
	float2 uv : TEXCOORD0;
};
cbuffer MaterialCB : register(b0)
{
	float4 diffuseColor;
};
Texture2D tex : register(t0);
SamplerState samp : register(s0);
float4 main(PS_IN pin) : SV_TARGET
{
	float4 sampled = tex.Sample(samp, pin.uv);
	if (all(sampled == 0.0f))
	{
		return diffuseColor;
	}
	return sampled * diffuseColor;
})EOT";
	*vs = new VertexShader();
	(*vs)->Compile(ModelVS);
	*ps = new PixelShader();
	(*ps)->Compile(ModelPS);
}



/*
* @brief コンストラクタ
*/
Model::Model()
	: m_loadScale(1.0f)
	, m_loadFlip(None)
	, m_playNo(ANIME_NONE)
	, m_blendNo(ANIME_NONE)
	, m_parametric{ANIME_NONE, ANIME_NONE}
	, m_blendTime(0.0f)
	, m_blendTotalTime(0.0f)
	, m_parametricBlend(0.0f)
{
	// デフォルトシェーダーの適用
	if (m_shaderRef == 0)
	{
		MakeModelDefaultShader(&m_pDefVS, &m_pDefPS);
	}
	m_pVS = m_pDefVS;
	m_pPS = m_pDefPS;
	++m_shaderRef;

	if (m_textureRef == 0)
	{
		m_pWhiteTex = new Texture();
		uint32_t white = 0xFFFFFFFF;
		HRESULT hr = m_pWhiteTex->Create(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, &white);
		if (FAILED(hr))
		{
			delete m_pWhiteTex;
			m_pWhiteTex = nullptr;
		}
	}
	++m_textureRef;
}

/*
* @brief デストラクタ
*/
Model::~Model()
{
	Reset();
	--m_shaderRef;
	if (m_shaderRef <= 0)
	{
		delete m_pDefPS;
		delete m_pDefVS;
	}

	--m_textureRef;
	if (m_textureRef <= 0)
	{
		delete m_pWhiteTex;
		m_pWhiteTex = nullptr;
	}
}

/*
* @brief 内部データ削除
*/
void Model::Reset()
{
	auto meshIt = m_meshes.begin();
	while (meshIt != m_meshes.end())
	{
		if (meshIt->pMesh) delete meshIt->pMesh;
		++meshIt;
	}

	auto matIt = m_materials.begin();
	while (matIt != m_materials.end())
	{
		if (matIt->pTexture) delete matIt->pTexture;
		++matIt;
	}
}

/*
* @brief 頂点シェーダー設定
*/
void Model::SetVertexShader(VertexShader* vs)
{
	m_pVS = vs;
}

/*
* @brief ピクセルシェーダー設定
*/
void Model::SetPixelShader(PixelShader* ps)
{
	m_pPS = ps;
}

/*
* @brief モデルデータ読み込み
* @param[in] file 読み込むモデルファイルへのパス
* @param[in] scale モデルのサイズ変更
* @param[in] flip 反転設定
* @return 読み込み結果
*/
bool Model::Load(const char* file, float scale, Flip flip)
{
#ifdef _DEBUG
        m_errorStr = "";
#endif
	Reset();

	// assimpの設定
	Assimp::Importer importer;
	int flag = 0;
	flag |= aiProcess_Triangulate;
	flag |= aiProcess_FlipUVs;

	// assimpで読み込み
	const aiScene* pScene = importer.ReadFile(file, flag);
	if (!pScene) {
#ifdef _DEBUG
		m_errorStr = importer.GetErrorString();
#endif
		return false;
	}

	// 読み込み時の設定を保存
	m_loadScale = scale;
	m_loadFlip = flip;

	// ディレクトリの読み取り
	std::string directory = file;
	auto strIt = directory.begin();
	while (strIt != directory.end()) {
		if (*strIt == '/')
			*strIt = '\\';
		++strIt;
	}
	directory = directory.substr(0, directory.find_last_of('\\') + 1);

	// ノードの作成
	MakeBoneNodes(pScene);
	// メッシュ作成
	MakeMesh(pScene, scale, flip);
	// マテリアルの作成
        MakeMaterial(pScene, directory);

        return true;
}

void Model::MakeMesh(const void* ptr, float scale, Flip flip)
{
        aiVector3D zero3(0.0f, 0.0f, 0.0f);
        aiColor4D one4(1.0f, 1.0f, 1.0f, 1.0f);
        const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);
        float xFlip = flip == Flip::XFlip ? -1.0f : 1.0f;
        float zFlip = (flip == Flip::ZFlip || flip == Flip::ZFlipUseAnime) ? -1.0f : 1.0f;
        int idx1 = (flip == Flip::XFlip || flip == Flip::ZFlip) ? 2 : 1;
        int idx2 = (flip == Flip::XFlip || flip == Flip::ZFlip) ? 1 : 2;

        //--- メッシュ作成
        m_meshes.resize(pScene->mNumMeshes);
        for (unsigned int i = 0; i < m_meshes.size(); ++i)
        {
                // 頂点配列の確保
                m_meshes[i].vertices.resize(pScene->mMeshes[i]->mNumVertices);

                // 頂点情報の設定
                for (unsigned int j = 0; j < m_meshes[i].vertices.size(); ++j) {
                        // 頂点データの取得
                        aiVector3D pos = pScene->mMeshes[i]->mVertices[j];
                        aiVector3D normal = pScene->mMeshes[i]->HasNormals() ?
                                pScene->mMeshes[i]->mNormals[j] : zero3;
                        aiVector3D uv = pScene->mMeshes[i]->HasTextureCoords(0) ?
                                pScene->mMeshes[i]->mTextureCoords[0][j] : zero3;
                        aiColor4D color = pScene->mMeshes[i]->HasVertexColors(0) ?
                                pScene->mMeshes[i]->mColors[0][j] : one4;

                        // 頂点の設定
                        m_meshes[i].vertices[j] = {
                                DirectX::XMFLOAT3(pos.x * scale * xFlip, pos.y * scale, pos.z * scale * zFlip),
                                DirectX::XMFLOAT3(normal.x, normal.y, normal.z),
                                DirectX::XMFLOAT2(uv.x, uv.y),
                                DirectX::XMFLOAT4(color.r, color.g, color.b, color.a)
                        };
                }

                // 重み設定
                MakeWeight(pScene, i);

                // インデックス配列の確保
                m_meshes[i].indices.resize(pScene->mMeshes[i]->mNumFaces * 3);

                // インデックスの設定
                for (unsigned int j = 0; j < pScene->mMeshes[i]->mNumFaces; ++j) {
                        aiFace face = pScene->mMeshes[i]->mFaces[j];
                        int idx = j * 3;
                        m_meshes[i].indices[idx + 0] = face.mIndices[0];
                        m_meshes[i].indices[idx + 1] = face.mIndices[idx1];
                        m_meshes[i].indices[idx + 2] = face.mIndices[idx2];
                }

                // マテリアル割り当て
                m_meshes[i].materialID = pScene->mMeshes[i]->mMaterialIndex;

                // バッファ生成用データ設定
                MeshBuffer::Description desc = {};
                desc.pVtx = m_meshes[i].vertices.data();
                desc.vtxSize = sizeof(Vertex);
                desc.vtxCount = m_meshes[i].vertices.size();
                desc.pIdx = m_meshes[i].indices.data();
                desc.idxSize = sizeof(unsigned long);
                desc.idxCount = m_meshes[i].indices.size();
                desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

                // バッファ作成
                m_meshes[i].pMesh = new MeshBuffer();
                m_meshes[i].pMesh->Create(desc);
        }
}

void Model::MakeMaterial(const void* ptr, std::string directory)
{
        aiColor3D color(0.0f, 0.0f, 0.0f);
        float shininess = 0.0f;
        const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);

        // マテリアルの作成
        m_materials.resize(pScene->mNumMaterials);
        for (unsigned int i = 0; i < m_materials.size(); ++i)
        {
                //--- マテリアルプロパティの読み込み
                // 拡散色の読み込み
                if (pScene->mMaterials[i]->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
                        m_materials[i].diffuse = DirectX::XMFLOAT4(color.r, color.g, color.b, 1.0f);
                else
                        m_materials[i].diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
                // 環境色の読み込み
                if (pScene->mMaterials[i]->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS)
                        m_materials[i].ambient = DirectX::XMFLOAT4(color.r, color.g, color.b, 1.0f);
                else
                        m_materials[i].ambient = DirectX::XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
                // 鏡面色の読み込み
                if (pScene->mMaterials[i]->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
                        m_materials[i].specular = DirectX::XMFLOAT4(color.r, color.g, color.b, 0.0f);
                else
                        m_materials[i].specular = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
                // 鏡面ハイライトの読み込み
                if (pScene->mMaterials[i]->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
                        m_materials[i].specular.w = shininess;

                // テクスチャの読み込み
                HRESULT hr = E_FAIL;
                aiString path;

                // テクスチャパス取得
                m_materials[i].pTexture = nullptr;
                if (pScene->mMaterials[i]->Get(AI_MATKEY_TEXTURE_DIFFUSE(0), path) != AI_SUCCESS) {
                        continue;
                }

                // 埋め込みテクスチャ
                if (path.length > 0 && path.C_Str()[0] == '*')
                {
                        unsigned int texIndex = std::atoi(path.C_Str() + 1);
                        if (texIndex < pScene->mNumTextures && pScene->mTextures[texIndex])
                        {
                                const aiTexture* pTexture = pScene->mTextures[texIndex];
                                Texture* texture = new Texture();
                                if (pTexture->mHeight == 0)
                                {
                                        const uint8_t* data = reinterpret_cast<const uint8_t*>(pTexture->pcData);
                                        hr = texture->CreateFromMemory(data, pTexture->mWidth, pTexture->achFormatHint);
                                }
                                else
                                {
                                        const uint8_t* data = reinterpret_cast<const uint8_t*>(pTexture->pcData);
                                        hr = texture->Create(DXGI_FORMAT_R8G8B8A8_UNORM, pTexture->mWidth, pTexture->mHeight, data);
                                }

                                if (SUCCEEDED(hr))
                                {
                                        m_materials[i].pTexture = texture;
                                        continue;
                                }
#ifdef _DEBUG
                                m_errorStr += "embedded texture load failed: ";
                                m_errorStr += path.C_Str();
                                m_errorStr += '\n';
#endif
                                delete texture;
                        }
#ifdef _DEBUG
                        else
                        {
                                m_errorStr += "embedded texture missing: ";
                                m_errorStr += path.C_Str();
                                m_errorStr += '\n';
                        }
#endif
                }

                Texture* texture = new Texture;

                // 直接読込
                hr = texture->Create(path.C_Str());
                if (SUCCEEDED(hr)) { m_materials[i].pTexture = texture; continue; }

                // ルートと結合して再試行
                hr = texture->Create((directory + path.C_Str()).c_str());
                if (SUCCEEDED(hr)) { m_materials[i].pTexture = texture; continue; }

                // ファイル名のみで再試行
                std::string fullPath = path.C_Str();
                std::string::iterator strIt = fullPath.begin();
                while (strIt != fullPath.end()) {
                        if (*strIt == '/')
                                *strIt = '\\';
                        ++strIt;
                }
                size_t find = fullPath.find_last_of("\\");
                std::string fileName = fullPath;
                if (find != std::string::npos)
                        fileName = fileName.substr(find + 1);
                hr = texture->Create((directory + fileName).c_str());
                if (SUCCEEDED(hr)) { m_materials[i].pTexture = texture; continue; }

                // テクスチャなし
                delete texture;
                m_materials[i].pTexture = nullptr;
#ifdef _DEBUG
                m_errorStr += path.C_Str();
#endif
        }
}

/*
* @brief 描画
* @param[in] meshNo 描画するメッシュ、-1は全部表示
* @param[in] func メッシュ描画コールバック
*/
void Model::Draw(int meshNo)
{
	// シェーダー設定
	m_pVS->Bind();
	m_pPS->Bind();

	// テクスチャ自動設定
	bool isAutoTexture = (meshNo == -1);

	// 描画数設定
	size_t drawNum = m_meshes.size();
	if (meshNo != -1)
		drawNum = meshNo + 1;
	else
		meshNo = 0;

	// 描画
	for (UINT i = meshNo; i < drawNum; ++i)
	{
		const auto& material = m_materials[m_meshes[i].materialID];

		if (m_pPS == m_pDefPS && m_pPS)
		{
			struct MaterialParam
			{
				DirectX::XMFLOAT4 diffuse;
			} param;
			param.diffuse = material.diffuse;
			m_pPS->WriteBuffer(0, &param);
		}

		if (isAutoTexture) {
			Texture* activeTex = material.pTexture ? material.pTexture : m_pWhiteTex;
			m_pPS->SetTexture(0, activeTex);
		}
		m_meshes[i].pMesh->Draw();
	}
}

/*
* @brief メッシュ情報取得
* @param[in] index メッシュ番号
* @return 該当メッシュ情報
*/
const Model::Mesh* Model::GetMesh(unsigned int index)
{
	if (index < GetMeshNum())
	{
		return &m_meshes[index];
	}
	return nullptr;
}

/*
* @brief メッシュ数取得
*/
uint32_t Model::GetMeshNum()
{
	return static_cast<uint32_t>(m_meshes.size());
}

/*
* @brief マテリアル情報取得
* @param[in] index マテリアル番号
* @return 該当マテリアル情報
*/
const Model::Material* Model::GetMaterial(unsigned int index)
{
	if (index < GetMaterialNum())
	{
		return &m_materials[index];
	}
	return nullptr;
}

/*
* @brief マテリアル数取得
*/
uint32_t Model::GetMaterialNum()
{
	return static_cast<uint32_t>(m_materials.size());
}

/*
* @brief アニメーション後の変換行列取得
* @param[in] index ボーン番号
* @return 該当ボーンの変換行列
*/
DirectX::XMMATRIX Model::GetBone(NodeIndex index)
{
	if (index < m_nodes.size())
	{
		return m_nodes[index].mat;
		
	}
	return DirectX::XMMatrixIdentity();
}

/*
* @brief アニメーション情報取得
* @param[in] no アニメ番号
* @return 該当アニメーション情報
*/
const Model::Animation* Model::GetAnimation(AnimeNo no)
{
	if (AnimeNoCheck(no))
	{
		return &m_animes[no];
	}
	return nullptr;
}


/*
* @brief アニメーション読み込み
* @param[in] file 読み込むアニメーションファイルへのパス
* @return 内部で割り当てられたアニメーション番号
*/
Model::AnimeNo Model::AddAnimation(const char* file)
{
#ifdef _DEBUG
	m_errorStr = "";
#endif

	// assimpの設定
	Assimp::Importer importer;
	int flag = 0;
	flag |= aiProcess_Triangulate;
	flag |= aiProcess_FlipUVs;
	if (m_loadFlip == Flip::XFlip)  flag |= aiProcess_MakeLeftHanded;

	// assimpで読み込み
	const aiScene* pScene = importer.ReadFile(file, flag);
	if (!pScene)
	{
#ifdef _DEBUG
		m_errorStr += importer.GetErrorString();
#endif
		return ANIME_NONE;
	}

	// アニメーションチェック
	if (!pScene->HasAnimations())
	{
#ifdef _DEBUG
		m_errorStr += "no animation.";
#endif
		return ANIME_NONE;
	}

	// アニメーションデータ確保
	aiAnimation* assimpAnime = pScene->mAnimations[0];
	m_animes.push_back(Animation());
	Animation& anime = m_animes.back();


	// アニメーション設定
	float animeFrame = static_cast<float>(assimpAnime->mTicksPerSecond);
	anime.totalTime = static_cast<float>(assimpAnime->mDuration )/ animeFrame;
	anime.channels.resize(assimpAnime->mNumChannels);
	Channels::iterator channelIt = anime.channels.begin();
	while (channelIt != anime.channels.end())
	{
		// 対応するチャンネル(ボーン)を探索
		uint32_t channelIdx = static_cast<uint32_t>(channelIt - anime.channels.begin());
		aiNodeAnim* assimpChannel = assimpAnime->mChannels[channelIdx];
		Model::Nodes::iterator nodeIt = std::find_if(m_nodes.begin(), m_nodes.end(),
			[assimpChannel](Node& node) {
				return node.name == assimpChannel->mNodeName.data;
			});
		if (nodeIt == m_nodes.end())
		{
			channelIt->index = INDEX_NONE;
			++ channelIt;
			continue;
		}

		// 各キーの値を設定
		channelIt->index = static_cast<NodeIndex>(nodeIt - m_nodes.begin());
		Timeline& timeline = channelIt->timeline;

		// 一度XMVECTOR型で格納
		using XMVectorKey = std::pair<float, DirectX::XMVECTOR>;
		using XMVectorKeys = std::map<float, DirectX::XMVECTOR>;
		XMVectorKeys keys[3];
		// 位置
		for (UINT i = 0; i < assimpChannel->mNumPositionKeys; ++i)
		{
			aiVectorKey& key = assimpChannel->mPositionKeys[i];
			keys[0].insert(XMVectorKey(static_cast<float>(key.mTime) / animeFrame,
					DirectX::XMVectorSet(key.mValue.x, key.mValue.y, key.mValue.z, 0.0f)
			));
		}
		// 回転
		for (UINT i = 0; i < assimpChannel->mNumRotationKeys; ++i)
		{
			aiQuatKey& key = assimpChannel->mRotationKeys[i];
			keys[1].insert(XMVectorKey(static_cast<float>(key.mTime) / animeFrame,
				DirectX::XMVectorSet(key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w)));
		}
		// 拡縮
		for (UINT i = 0; i < assimpChannel->mNumScalingKeys; ++i)
		{
			aiVectorKey& key = assimpChannel->mScalingKeys[i];
			keys[2].insert(XMVectorKey(static_cast<float>(key.mTime) / animeFrame,
				DirectX::XMVectorSet(key.mValue.x, key.mValue.y, key.mValue.z, 0.0f)));
		}

		// 各タイムラインの先頭の参照を設定
		XMVectorKeys::iterator it[] = {keys[0].begin(), keys[1].begin(), keys[2].begin()};
		for (int i = 0; i < 3; ++i)
		{
			// キーが一つしかない場合は、参照終了
			if (keys[i].size() == 1)
				++ it[i];
		}

		// 各要素ごとのタイムラインではなく、すべての変換を含めたタイムラインの作成
		while (it[0] != keys[0].end() && it[1] != keys[1].end() && it[2] != keys[2].end())
		{
			// 現状の参照位置で一番小さい時間を取得
			float time = anime.totalTime;
			for (int i = 0; i < 3; ++i)
			{
				if (it[i] != keys[i].end())
				{
					time = std::min(it[i]->first, time);
				}
			}

			// 時間に基づいて補間値を計算
			DirectX::XMVECTOR result[3];
			for (int i = 0; i < 3; ++i)
			{
				// 先頭のキーより小さい時間であれば、先頭の値を設定
				if (time < keys[i].begin()->first)
				{
					result[i] = keys[i].begin()->second;
				}
				// 最終キーより大きい時間であれば、最終の値を設定
				else if (keys[i].rbegin()->first <= time)
				{
					result[i] = keys[i].rbegin()->second;
					it[i] = keys[i].end();
				}
				// キー同士に挟まれた時間であれば、補間値を計算
				else
				{
					// 参照している時間と同じであれば、次の参照へキーを進める
					if (it[i]->first <= time)
					{
						++it[i];
					}

					// 補間値の計算
					XMVectorKeys::iterator prev = it[i];
					--prev;
					float rate = (time - prev->first) / (it[i]->first - prev->first);
					result[i] = DirectX::XMVectorLerp(prev->second, it[i]->second, rate);
				}
			}

			// 指定時間に基づいたキーを追加
			Transform transform;
			DirectX::XMStoreFloat3(&transform.translate, result[0]);
			DirectX::XMStoreFloat4(&transform.quaternion, result[1]);
			DirectX::XMStoreFloat3(&transform.scale, result[2]);
			timeline.insert(Key(time, transform));
		}

		++ channelIt;
	}


	// アニメ番号を返す
	return static_cast<AnimeNo>(m_animes.size() - 1);
}

/*
* @brief アニメーションの更新処理
* @param[in] tick アニメーション経過時間
*/
void Model::Step(float tick)
{
	// アニメーションの再生確認
	if (m_playNo == ANIME_NONE) { return; }

	//--- アニメーション行列の更新
	// パラメトリック
	if (m_playNo == PARAMETRIC_ANIME || m_blendNo == PARAMETRIC_ANIME)
	{
		CalcAnime(PARAMETRIC0, m_parametric[0]);
		CalcAnime(PARAMETRIC1, m_parametric[1]);
	}
	// メインアニメ
	if (m_playNo != ANIME_NONE && m_playNo != PARAMETRIC_ANIME)
	{
		CalcAnime(MAIN, m_playNo);
	}
	// ブレンドアニメ
	if (m_blendNo != ANIME_NONE && m_blendNo != PARAMETRIC_ANIME)
	{
		CalcAnime(BLEND, m_blendNo);
	}

	// アニメーション行列に基づいて骨行列を更新
	CalcBones(0, DirectX::XMMatrixScaling(m_loadScale, m_loadScale, m_loadScale));

	//--- アニメーションの時間更新
	// メインアニメ
	UpdateAnime(m_playNo, tick);
	// ブレンドアニメ
	if (m_blendNo != ANIME_NONE)
	{
		UpdateAnime(m_blendNo, tick);
		m_blendTime += tick;
		if (m_blendTime <= m_blendTime)
		{
			// ブレンドアニメの自動終了
			m_blendTime = 0.0f;
			m_blendTotalTime = 0.0f;
			m_playNo = m_blendNo;
			m_blendNo = ANIME_NONE;
		}
	}
	// パラメトリック
	if (m_playNo == PARAMETRIC_ANIME || m_blendNo == PARAMETRIC_ANIME)
	{
		UpdateAnime(m_parametric[0], tick);
		UpdateAnime(m_parametric[1], tick);
	}
}

/*
* @brief アニメーション再生
* @param[in] no 再生するアニメーション番号
* @param[in] loop ループ再生フラグ
* @param[in] speed 再生速度
*/
void Model::Play(AnimeNo no, bool loop, float speed)
{
	// 再生チェック
	if (!AnimeNoCheck(no)) { return; }
	if (m_playNo == no) { return; }

	// 合成アニメーションかチェック
	if (no != PARAMETRIC_ANIME)
	{
		// 通常の初期化
		InitAnime(no);
		m_animes[no].isLoop = loop;
		m_animes[no].speed = speed;
	}
	else
	{
		// 合成アニメーションの元になっているアニメーションを初期化
		InitAnime(m_parametric[0]);
		InitAnime(m_parametric[1]);
		m_animes[m_parametric[0]].isLoop = loop;
		m_animes[m_parametric[1]].isLoop = loop;
		SetParametricBlend(0.0f);
	}

	// 再生アニメーションの設定
	m_playNo = no;
}

/*
* @brief ブレンド再生
* @param[in] no アニメーション番号
* @param[in] blendTime ブレンドに掛ける時間
* @param[in] loop ループフラグ
* @param[in] speed 再生速度
*/
void Model::PlayBlend(AnimeNo no, float blendTime, bool loop, float speed)
{
	// 再生チェック
	if (!AnimeNoCheck(no)) { return; }

	// 合成アニメーションかチェック
	if (no != PARAMETRIC_ANIME)
	{
		InitAnime(no);
		m_animes[no].isLoop = loop;
		m_animes[no].speed = speed;
	}
	else
	{
		// 合成アニメーションの元になっているアニメーションを初期化
		InitAnime(m_parametric[0]);
		InitAnime(m_parametric[1]);
		m_animes[m_parametric[0]].isLoop = loop;
		m_animes[m_parametric[1]].isLoop = loop;
		SetParametricBlend(0.0f);
	}

	// ブレンドの設定
	m_blendTime = 0.0f;
	m_blendTotalTime = blendTime;
	m_blendNo = no;
}

/*
* @brief 合成元アニメーションの設定
* @param[in] no1 合成元アニメ1
* @param[in] no2 合成元アニメ2
*/
void Model::SetParametric(AnimeNo no1, AnimeNo no2)
{
	// アニメーションチェック
	if (!AnimeNoCheck(no1)) { return; }
	if (!AnimeNoCheck(no2)) { return; }

	// 合成設定
	m_parametric[0] = no1;
	m_parametric[1] = no2;
	SetParametricBlend(0.0f);
}

/*
* @brief アニメーションの合成割合設定
* @param[in] blendRate 合成割合
*/
void Model::SetParametricBlend(float blendRate)
{
	// 合成元アニメが設定されているか確認
	if (m_parametric[0] == ANIME_NONE || m_parametric[1] == ANIME_NONE) return;

	// 合成割合設定
	m_parametricBlend = blendRate;

	// 割合に基づいてアニメーションの再生速度を設定
	Animation& anime1 = m_animes[m_parametric[0]];
	Animation& anime2 = m_animes[m_parametric[1]];
	float blendTotalTime = anime1.totalTime * (1.0f - m_parametricBlend) + anime2.totalTime * m_parametricBlend;
	anime1.speed = anime1.totalTime / blendTotalTime;
	anime2.speed = anime2.totalTime / blendTotalTime;
}

/*
* @brief アニメーションの再生時間を変更
* @param[in] no 変更するアニメ
* @param[in] time 新しい再生時間
*/
void Model::SetAnimationTime(AnimeNo no, float time)
{
	// アニメーションチェック
	if (!AnimeNoCheck(no)) { return; }

	// 再生時間変更
	Animation& anime = m_animes[no];
	anime.nowTime = time;
	while (anime.nowTime >= anime.totalTime)
	{
		anime.nowTime -= anime.totalTime;
	}
}

/*
* @brief 再生フラグの取得
* @param[in] no 調べるアニメ番号
* @return 現在再生中ならtrue
*/
bool Model::IsPlay(AnimeNo no)
{
	// アニメーションチェック
	if (!AnimeNoCheck(no)) { return false; }

	// パラメトリックは合成元のアニメを基準に判断
	if (no == PARAMETRIC_ANIME) { no = m_parametric[0]; }

	// 再生時間の判定
	if (m_animes[no].totalTime < m_animes[no].nowTime) { return false; }

	// それぞれの再生番号に設定されているか確認
	if (m_playNo == no) { return true; }
	if (m_blendNo == no) { return true; }
	if (m_playNo == PARAMETRIC_ANIME || m_blendNo == PARAMETRIC_ANIME)
	{
		if (m_parametric[0] == no) { return true; }
		if (m_parametric[1] == no) { return true; }
	}

	// 再生中でない
	return false;
}

/*
* @brief 再生中の番号の取得
* @return アニメ番号
*/
Model::AnimeNo Model::GetPlayNo()
{
	return m_playNo;
}

/*
* @brief 再生中のブレンドアニメの取得
* @return アニメ番号
*/
Model::AnimeNo Model::GetBlendNo()
{
	return m_blendNo;
}


#ifdef _DEBUG

/*
* @brief エラーメッセージ取得
* @returnn エラーメッセージ
*/
std::string Model::GetError()
{
	return m_errorStr;
}

/*
* @brief ボーンデバッグ描画
*/
void Model::DrawBone()
{
	// 再帰処理
	std::function<void(int, DirectX::XMFLOAT3)> FuncDrawBone =
		[&FuncDrawBone, this](int idx, DirectX::XMFLOAT3 parent)
	{
		// 親ノードから現在位置まで描画
		DirectX::XMFLOAT3 pos;
		DirectX::XMStoreFloat3(&pos, DirectX::XMVector3TransformCoord(DirectX::XMVectorZero(), m_nodes[idx].mat));
		Geometory::AddLine(parent, pos, DirectX::XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f));

		// 子ノードの描画
		auto it = m_nodes[idx].children.begin();
		while (it != m_nodes[idx].children.end())
		{
			FuncDrawBone(*it, pos);
			++it;
		}
	};

	// 描画実行
	FuncDrawBone(0, DirectX::XMFLOAT3());
	Geometory::DrawLines();
}

#endif


void Model::MakeBoneNodes(const void* ptr)
{
	// 再帰処理でAssimpのノード情報を読み取り
	std::function<NodeIndex(aiNode*, NodeIndex, DirectX::XMMATRIX mat)> FuncAssimpNodeConvert =
		[&FuncAssimpNodeConvert, this](aiNode* assimpNode, NodeIndex parent, DirectX::XMMATRIX mat)
	{
		DirectX::XMMATRIX transform = GetMatrixFromAssimpMatrix(assimpNode->mTransformation);
		std::string name = assimpNode->mName.data;
		if (name.find("$AssimpFbx") != std::string::npos)
		{
			mat = transform * mat;
			return FuncAssimpNodeConvert(assimpNode->mChildren[0], parent, mat);
		}
		else
		{
			// Assimpのノード情報をモデルクラスへ格納
			Node node;
			node.name = assimpNode->mName.data;
			node.parent = parent;
			node.children.resize(assimpNode->mNumChildren);
			node.mat = mat;

			// ノードリストに追加
			m_nodes.push_back(node);
			NodeIndex nodeIndex = static_cast<NodeIndex>(m_nodes.size() - 1);

			// 子要素も同様に変換
			for (UINT i = 0; i < assimpNode->mNumChildren; ++i)
			{
				m_nodes[nodeIndex].children[i] = FuncAssimpNodeConvert(
					assimpNode->mChildren[i], nodeIndex, DirectX::XMMatrixIdentity());
			}
			return nodeIndex;
		}
	};

	// ノード作成
	m_nodes.clear();
	FuncAssimpNodeConvert(reinterpret_cast<const aiScene*>(ptr)->mRootNode, INDEX_NONE, DirectX::XMMatrixIdentity());

	// アニメーション計算領域に、ノード数分の初期データを作成
	Transform init = {
		DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
		DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f)
	};
	for (int i = 0; i < MAX_TRANSFORM; ++i)
	{
		m_nodeTransform[i].resize(m_nodes.size(), init);
	}
}

void Model::MakeWeight(const void* ptr, int meshIdx)
{
	const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);

	// メッシュに対応するボーンがあるか
	aiMesh* assimpMesh = pScene->mMeshes[meshIdx];
	Mesh& mesh = m_meshes[meshIdx];
	if (assimpMesh->HasBones())
	{
		// メッシュ内の頂点領域作成
		struct WeightPair
		{
			unsigned int idx;
			float weight;
		};
		std::vector<std::vector<WeightPair>> weights;
		weights.resize(mesh.vertices.size());


		// メッシュに割り当てられているボーン領域確保
		mesh.bones.resize(assimpMesh->mNumBones);
		for (auto boneIt = mesh.bones.begin(); boneIt != mesh.bones.end(); ++boneIt)
		{
			UINT boneIdx = static_cast<UINT>(boneIt - mesh.bones.begin());
			aiBone* assimpBone = assimpMesh->mBones[boneIdx];
			// 構築済みのボーンノードから該当ノードを取得
			std::string boneName = assimpBone->mName.data;
			auto nodeIt = std::find_if(m_nodes.begin(), m_nodes.end(),
				[boneName](const Node& val) {
				return val.name == boneName;
			});
			// メッシュに割り当てられているボーンが、ノードに存在しない
			if (nodeIt == m_nodes.end())
			{
				boneIt->index = INDEX_NONE;
				continue;
			}

			// メッシュのボーンとノードの紐づけ
			boneIt->index = static_cast<NodeIndex>(nodeIt - m_nodes.begin());
			boneIt->invOffset = GetMatrixFromAssimpMatrix(assimpBone->mOffsetMatrix);
			boneIt->invOffset.r[3].m128_f32[0] *= m_loadScale;
			boneIt->invOffset.r[3].m128_f32[1] *= m_loadScale;
			boneIt->invOffset.r[3].m128_f32[2] *= m_loadScale;
			boneIt->invOffset =
				DirectX::XMMatrixScaling(m_loadFlip == ZFlipUseAnime ? -1.0f : 1.0f, 1.0f, 1.0f) *
				boneIt->invOffset * 
				DirectX::XMMatrixScaling(1.f / m_loadScale, 1.f / m_loadScale, 1.f / m_loadScale);

			// ウェイトの設定
			UINT weightNum = assimpBone->mNumWeights;
			for (UINT i = 0; i < weightNum; ++i)
			{
				aiVertexWeight weight = assimpBone->mWeights[i];
				weights[weight.mVertexId].push_back({boneIdx, weight.mWeight});
			}
		}

		// 取得してきた頂点ウェイトを設定
		for (int i = 0; i < weights.size(); ++i)
		{
			if (weights[i].size() >= 4)
			{
				std::sort(weights[i].begin(), weights[i].end(), [](WeightPair& a, WeightPair& b) {
					return a.weight > b.weight;
				});
				// ウェイト数4に合わせて正規化
				float total = 0.0f;
				for (int j = 0; j < 4; ++j)
					total += weights[i][j].weight;
				for (int j = 0; j < 4; ++j)
					weights[i][j].weight /= total;
			}
			for (int j = 0; j < weights[i].size() && j < 4; ++j)
			{
				mesh.vertices[i].index[j] = weights[i][j].idx;
				mesh.vertices[i].weight[j] = weights[i][j].weight;
			}
		}
	}
	else
	{
		// メッシュの親ノードをトランスフォーム元として計算
		std::string nodeName = assimpMesh->mName.data;
		auto nodeIt = std::find_if(m_nodes.begin(), m_nodes.end(),
			[nodeName](const Node& val) {
				return val.name == nodeName;
			});
		if (nodeIt == m_nodes.end())
		{
			return;	// ボーンデータなし
		}

		// メッシュでない親ノードを再帰探索
		std::function<int(int)> FuncFindNode =
			[&FuncFindNode, this, pScene](NodeIndex parent)
		{
			std::string name = m_nodes[parent].name;
			for (UINT i = 0; i < pScene->mNumMeshes; ++i)
			{
				if (name == pScene->mMeshes[i]->mName.data)
				{
					return FuncFindNode(m_nodes[parent].parent);
				}
			}
			return parent;
		};

		Bone bone;
		bone.index = FuncFindNode(nodeIt->parent);
		bone.invOffset = DirectX::XMMatrixInverse(nullptr, m_nodes[bone.index].mat);
		for (auto vtxIt = mesh.vertices.begin(); vtxIt != mesh.vertices.end(); ++vtxIt)
		{
			vtxIt->weight[0] = 1.0f;
		}

		mesh.bones.resize(1);
		mesh.bones[0] = bone;
	}
}



bool Model::AnimeNoCheck(AnimeNo no)
{
	// パラメトリックアニメーション確認
	if (no == PARAMETRIC_ANIME)
	{
		// パラメトリックのアニメーションが両方正しく設定されているか
		return
			m_parametric[0] != ANIME_NONE &&
			m_parametric[1] != ANIME_NONE;
	}
	else
	{
		// 問題ないアニメーション番号かどうか
		return 0 <= no && no < m_animes.size();
	}
}
void Model::InitAnime(AnimeNo no)
{
	// アニメの設定なし、パラメトリックで設定されているなら初期化しない
	if (no == ANIME_NONE || no == PARAMETRIC_ANIME) { return; }

	Animation& anime = m_animes[no];
	anime.nowTime = 0.0f;
	anime.speed = 1.0f;
	anime.isLoop = false;
}
void Model::CalcAnime(AnimeTransform kind, AnimeNo no)
{
	Animation& anime = m_animes[no];
	Channels::iterator channelIt = anime.channels.begin();
	while (channelIt != anime.channels.end())
	{
		// 一致するボーンがなければスキップ
		Timeline& timeline = channelIt->timeline;
		if (channelIt->index == INDEX_NONE || timeline.empty())
		{
			++channelIt;
			continue;
		}

		//--- 該当ノードの姿勢をアニメーションで更新
		Transform& transform = m_nodeTransform[kind][channelIt->index];
		if (timeline.size() <= 1)
		{
			// キーが一つしかないので値をそのまま使用
			transform = channelIt->timeline[0];
		}
		else
		{
			Timeline::iterator startIt = timeline.begin();
			if (anime.nowTime <= startIt->first)
			{
				// 先頭キーよりも前の時間なら、先頭の値を使用
				transform = startIt->second;

				
			}
			else if (timeline.rbegin()->first <= anime.nowTime)
			{
				// 最終キーよりも後の時間なら、最後の値を使用
				transform = timeline.rbegin()->second;
			}
			else
			{
				// 指定された時間を挟む2つのキーから、補間された値を計算
				Timeline::iterator nextIt = timeline.upper_bound(anime.nowTime);
				startIt = nextIt;
				--startIt;
				float rate = (anime.nowTime - startIt->first) / (nextIt->first - startIt->first);
				LerpTransform(&transform, startIt->second, nextIt->second, rate);

				
			}
		}

		++channelIt;
	}
}
void Model::UpdateAnime(AnimeNo no, float tick)
{
	if (no == PARAMETRIC_ANIME) { return; }

	Animation& anime = m_animes[no];
	anime.nowTime += anime.speed * tick;
	if (anime.isLoop)
	{
		while (anime.nowTime >= anime.totalTime)
		{
			anime.nowTime -= anime.totalTime;
		}
	}
}
void Model::CalcBones(NodeIndex index, const DirectX::XMMATRIX parent)
{
	//--- アニメーションごとのパラメータを合成
	Transform transform;
	// パラメトリック
	if (m_playNo == PARAMETRIC_ANIME || m_blendNo == PARAMETRIC_ANIME)
	{
		LerpTransform(&transform, m_nodeTransform[PARAMETRIC0][index], m_nodeTransform[PARAMETRIC1][index], m_parametricBlend);
		if (m_playNo == PARAMETRIC_ANIME)
		{
			m_nodeTransform[MAIN][index] = transform;
		}
		if (m_blendNo == PARAMETRIC_ANIME)
		{
			m_nodeTransform[BLEND][index] = transform;
		}
	}
	// ブレンドアニメ
	if (m_blendNo != ANIME_NONE)
	{
		LerpTransform(&transform, m_nodeTransform[MAIN][index], m_nodeTransform[BLEND][index], m_blendTime / m_blendTotalTime);
	}
	else
	{
		// メインアニメのみ
		transform = m_nodeTransform[MAIN][index];
	}

	// 該当ノードの姿勢行列を計算
	Node& node = m_nodes[index];
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&transform.translate));
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&transform.quaternion));
	DirectX::XMMATRIX S = DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&transform.scale));
	node.mat = (S * R * T) * parent;

	// 子要素の姿勢を更新
	Children::iterator it = node.children.begin();
	while (it != node.children.end())
	{
		CalcBones(*it, node.mat);
		++it;
	}
}

void Model::LerpTransform(Transform* pOut, const Transform& a, const Transform& b, float rate)
{
	DirectX::XMVECTOR vec[][2] = {
		{ DirectX::XMLoadFloat3(&a.translate),	DirectX::XMLoadFloat3(&b.translate) },
		{ DirectX::XMLoadFloat4(&a.quaternion),	DirectX::XMLoadFloat4(&b.quaternion) },
		{ DirectX::XMLoadFloat3(&a.scale),		DirectX::XMLoadFloat3(&b.scale) },
	};
	vec[0][0] = DirectX::XMVectorLerp(vec[0][0], vec[0][1], rate);
	vec[1][0] = DirectX::XMQuaternionSlerp(vec[1][0], vec[1][1], rate);
	vec[2][0] = DirectX::XMVectorLerp(vec[2][0], vec[2][1], rate);
	
	DirectX::XMStoreFloat3(&pOut->translate, vec[0][0]);
	DirectX::XMStoreFloat4(&pOut->quaternion, vec[1][0]);
	DirectX::XMStoreFloat3(&pOut->scale, vec[2][0]);
}

void Test()
{
	
}
