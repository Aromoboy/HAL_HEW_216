#include "Model.h"

#include "ShaderList.h"
#include <algorithm>
#include <cassert>
#include <sstream>
#include <string>

#if defined(_WIN32)
                #ifndef NOMINMAX
                #define NOMINMAX 1
#endif
#include <windows.h>
// Windows headers sometimes define max/min macros which conflict with std::max/std::min.
#undef max
#undef min
#endif
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

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
VertexShader*   Model::m_pDefVS         = nullptr;
PixelShader*    Model::m_pDefPS         = nullptr;
unsigned int    Model::m_shaderRef      = 0;
#ifdef _DEBUG
std::string             Model::m_errorStr       = "";
#endif

/*
* @brief デフォルトのシェーダーを作成
* @param[out] vs 頂点シェーダー格納先
* @param[out] ps ピクセルシェーダー格納先
*/
void MakeModelDefaultShader(VertexShader** vs, PixelShader** ps)
{
const char* ModelVS = R"EOT(
struct VS_IN {
float3 pos : POSITION;
float2 uv : TEXCOORD0;
float3 normal : NORMAL0;
};
struct VS_OUT {
float4 pos : SV_POSITION;
float3 normal : NORMAL0;
        float2 uv : TEXCOORD0;
};
cbuffer WVP : register(b0) {
        float4x4 world;
        float4x4 view;
        float4x4 proj;
        float4 normalBasis[3];
};
VS_OUT main(VS_IN vin) {
        VS_OUT vout;
        float4 worldPos = mul(float4(vin.pos, 1.0f), world);
        float4 viewPos = mul(worldPos, view);
        vout.pos = mul(viewPos, proj);
        vout.uv = vin.uv;
        float3x3 normalMat = float3x3(normalBasis[0].xyz, normalBasis[1].xyz, normalBasis[2].xyz);
        vout.normal = normalize(mul(vin.normal, normalMat));
        return vout;
})EOT";
        const char* ModelPS = R"EOT(
struct PS_IN {
        float4 pos : SV_POSITION;
        float3 normal : NORMAL0;
        float2 uv : TEXCOORD0;
};
Texture2D tex : register(t0);
SamplerState samp : register(s0);
float4 main(PS_IN pin) : SV_TARGET
{
        return tex.Sample(samp, pin.uv);
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
{
        // デフォルトシェーダーの適用
        if (m_shaderRef == 0)
        {
                MakeModelDefaultShader(&m_pDefVS, &m_pDefPS);
        }
        m_pVS = m_pDefVS;
        m_pPS = m_pDefPS;
        ++m_shaderRef;
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
        m_meshes.clear();

        auto matIt = m_materials.begin();
        while (matIt != m_materials.end())
        {
                if (matIt->pTexture) delete matIt->pTexture;
                ++matIt;
        }
        m_materials.clear();
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
        //flag |= aiProcess_MakeLeftHanded;

        // assimpで読み込み
        const aiScene* pScene = importer.ReadFile(file, flag);
        if (!pScene) {
#ifdef _DEBUG
                m_errorStr = importer.GetErrorString();
#endif
                return false;
        }

        // ディレクトリの読み取り
        std::string directory = file;
        auto strIt = directory.begin();
        while (strIt != directory.end()) {
                if (*strIt == '/')
                        *strIt = '\\';
                ++strIt;
        }
        directory = directory.substr(0, directory.find_last_of('\\') + 1);

        // メッシュ作成
        MakeMesh(pScene, scale, flip);
        // マテリアルの作成
        MakeMaterial(pScene, directory);

        return true;
}

/*
* @brief メッシュ作成
*/
void Model::MakeMesh(const void* ptr, float scale, Flip flip)
{
        const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);
        if (!pScene)
        {
                return;
        }

        m_meshes.resize(pScene->mNumMeshes);

        const bool flipX = flip == Flip::XFlip;
        const bool flipZ = flip == Flip::ZFlip || flip == Flip::ZFlipUseAnime;
        const bool needReverseWinding = flipX || flipZ;

        for (unsigned int i = 0; i < pScene->mNumMeshes; ++i)
        {
                const aiMesh* mesh = pScene->mMeshes[i];
                if (!mesh)
                {
                        continue;
                }

                Mesh modelMesh{};
                modelMesh.materialID = mesh->mMaterialIndex;

                // 頂点情報の取得
                modelMesh.vertices.resize(mesh->mNumVertices);
                for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
                {
                        aiVector3D pos = mesh->mVertices[j];
                        aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][j] : aiVector3D(0, 0, 0);
                        aiVector3D normal = mesh->HasNormals() ? mesh->mNormals[j] : aiVector3D(0, 0, 0);

                        if (flipX) { pos.x = -pos.x; normal.x = -normal.x; }
                        if (flipZ) { pos.z = -pos.z; normal.z = -normal.z; }

                        Vertex v{};
                        v.pos = DirectX::XMFLOAT3(pos.x * scale, pos.y * scale, pos.z * scale);
                        v.uv = DirectX::XMFLOAT2(uv.x, uv.y);
                        v.normal = DirectX::XMFLOAT3(normal.x, normal.y, normal.z);
                        modelMesh.vertices[j] = v;
                }

                // インデックスの取得
                modelMesh.indices.resize(mesh->mNumFaces * 3);
                for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
                {
                        const aiFace& face = mesh->mFaces[j];
                        const size_t idx = static_cast<size_t>(j) * 3;
                        modelMesh.indices[idx + 0] = face.mIndices[0];
                        modelMesh.indices[idx + 1] = face.mIndices[1];
                        modelMesh.indices[idx + 2] = face.mIndices[2];

                        if (needReverseWinding)
                        {
                                std::swap(modelMesh.indices[idx + 1], modelMesh.indices[idx + 2]);
                        }
                }

                // バッファ生成
                MeshBuffer::Description desc{};
                desc.pVtx = modelMesh.vertices.data();
                desc.vtxSize = sizeof(Vertex);
                desc.vtxCount = static_cast<UINT>(modelMesh.vertices.size());
                desc.isWrite = false;
                desc.pIdx = modelMesh.indices.data();
                desc.idxSize = sizeof(Indices::value_type);
                desc.idxCount = static_cast<UINT>(modelMesh.indices.size());
                desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

                modelMesh.pMesh = new MeshBuffer();
                if (FAILED(modelMesh.pMesh->Create(desc)))
                {
                        delete modelMesh.pMesh;
                        modelMesh.pMesh = nullptr;
                }

                m_meshes[i] = modelMesh;
        }
}

/*
* @brief マテリアル作成
*/
void Model::MakeMaterial(const void* ptr, std::string directory)
{
        const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);
        if (!pScene)
        {
                return;
        }

        m_materials.resize(pScene->mNumMaterials);

        // ディレクトリ区切りを統一
        for (auto& c : directory)
        {
                if (c == '/')
                {
                        c = '\\';
                }
        }

        for (unsigned int i = 0; i < pScene->mNumMaterials; ++i)
        {
                Material material{};
                aiColor3D col;
                float shininess = 0.0f;

                if (pScene->mMaterials[i]->Get(AI_MATKEY_COLOR_DIFFUSE, col) == AI_SUCCESS)
                        material.diffuse = { col.r, col.g, col.b, 1.0f };
                else
                        material.diffuse = { 1, 1, 1, 1 };

                if (pScene->mMaterials[i]->Get(AI_MATKEY_COLOR_AMBIENT, col) == AI_SUCCESS)
                        material.ambient = { col.r, col.g, col.b, 1.0f };
                else
                        material.ambient = { 0.3f, 0.3f, 0.3f, 1.0f };

                if (pScene->mMaterials[i]->Get(AI_MATKEY_COLOR_SPECULAR, col) == AI_SUCCESS)
                        material.specular = { col.r, col.g, col.b, 0.0f };
                else
                        material.specular = { 0, 0, 0, 0 };

                if (pScene->mMaterials[i]->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
                        material.specular.w = shininess;

                aiString texPath;
                if (pScene->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
                {
                        std::string texFile = texPath.C_Str();
                        if (!texFile.empty() && texFile[0] != '*')
                        {
                                std::string fullPath = directory + texFile;
                                Texture* pTex = new Texture();
                                if (SUCCEEDED(pTex->Create(fullPath.c_str())))
                                {
                                        material.pTexture = pTex;
                                }
                                else
                                {
                                        delete pTex;
                                        material.pTexture = nullptr;
                                }
                        }
                }

                m_materials[i] = material;
        }
}

/*
* @brief 描画
* @param[in] meshNo 描画するメッシュ番号
*/
void Model::Draw(const WorldBuffer& wvp, int meshNo)
{
        if (m_meshes.empty())
        {
                OutputDebugStringA("[Model] Draw called with no meshes loaded.\n");
                assert(!m_meshes.empty());
                return;
        }

        if (meshNo < 0)
        {
                OutputDebugStringA("[Model] Draw called with negative mesh index.\n");
                assert(meshNo >= 0);
                return;
        }

        const size_t index = static_cast<size_t>(meshNo);
        if (index >= m_meshes.size())
        {
                std::ostringstream oss;
                oss << "[Model] Draw requested mesh index " << meshNo << " but only " << m_meshes.size()
                        << " mesh(es) are available.";
                OutputDebugStringA((oss.str() + "\n").c_str());
                assert(index < m_meshes.size());
                return;
        }

        const Mesh& mesh = m_meshes[index];
        if (!mesh.pMesh)
        {
                std::ostringstream oss;
                oss << "[Model] Mesh buffer missing for index " << meshNo << ".";
                OutputDebugStringA((oss.str() + "\n").c_str());
                assert(mesh.pMesh);
                return;
        }

        ShaderList::SetWVP(wvp);
        // シェーダー設定
        m_pVS->Bind();
        m_pPS->Bind();

        mesh.pMesh->Draw();
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

#ifdef _DEBUG

/*
* @brief エラーメッセージ取得
* @returnn エラーメッセージ
*/
std::string Model::GetError()
{
        return m_errorStr;
}

#endif
