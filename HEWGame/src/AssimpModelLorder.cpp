#include "AssimpModelLorder.h"

#include <DirectXMath.h>
#include <memory>
#include <string>
#include <vector>
#include "TextureCache.h"
#include "TextureDX.h"
#include "ModelDX.h"
#include "MeshDX.h"
#include "MaterialDX.h"

#if defined(_WIN32)
#    include <windows.h>
#endif

AssimpModelLorder::AssimpModelLorder() = default;

// モデル読み込み
std::shared_ptr<ModelDX> AssimpModelLorder::Load(
    const std::string& path,
    ID3D11Device* device,
    TextureCache& textureCache)
{
    Assimp::Importer importer;
    unsigned int flag = aiProcess_Triangulate | aiProcess_FlipUVs;
    const aiScene* pScene = importer.ReadFile(path.c_str(), flag);
    if (!pScene)
        return nullptr;

    // ディレクトリ抽出
    std::string directory = path;
    for (auto& c : directory)
        if (c == '/')
            c = '\\';
    directory = directory.substr(0, directory.find_last_of('\\') + 1);

    // メッシュ作成
    auto meshes = MakeMesh(pScene);

    // マテリアル作成
    auto materials = MakeMaterial(pScene, directory, device, textureCache);

    // モデル生成
    return std::make_shared<ModelDX>(meshes, materials);
}

// メッシュ作成
std::vector<std::shared_ptr<MeshDX>>
AssimpModelLorder::MakeMesh(const aiScene* ptr)
{
    const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);
    std::vector<std::shared_ptr<MeshDX>> meshes;
    meshes.resize(pScene->mNumMeshes);

    for (unsigned int i = 0; i < meshes.size(); ++i)
    {
        std::vector<MeshDX::Vertex> vertices;
        std::vector<unsigned long> indices;

        // 頂点
        vertices.resize(pScene->mMeshes[i]->mNumVertices);
        for (unsigned int j = 0; j < vertices.size(); ++j)
        {
            aiVector3D pos = pScene->mMeshes[i]->mVertices[j];
            aiVector3D normal = pScene->mMeshes[i]->HasNormals()
                ? pScene->mMeshes[i]->mNormals[j]
                : aiVector3D(0, 0, 0);
            aiVector3D uv = pScene->mMeshes[i]->HasTextureCoords(0)
                ? pScene->mMeshes[i]->mTextureCoords[0][j]
                : aiVector3D(0, 0, 0);
            aiColor4D col = pScene->mMeshes[i]->HasVertexColors(0)
                ? pScene->mMeshes[i]->mColors[0][j]
                : aiColor4D(1, 1, 1, 1);

            vertices[j] = {
                DirectX::XMFLOAT3(pos.x, pos.y, pos.z),
                DirectX::XMFLOAT3(normal.x, normal.y, normal.z),
                DirectX::XMFLOAT2(uv.x, uv.y),
                DirectX::XMFLOAT4(col.r, col.g, col.b, col.a)
            };
        }

        // インデックス
        indices.resize(pScene->mMeshes[i]->mNumFaces * 3);
        for (unsigned int j = 0; j < pScene->mMeshes[i]->mNumFaces; ++j)
        {
            aiFace face = pScene->mMeshes[i]->mFaces[j];
            int idx = j * 3;
            indices[idx + 0] = face.mIndices[0];
            indices[idx + 1] = face.mIndices[1];
            indices[idx + 2] = face.mIndices[2];
        }

        // マテリアル番号
        unsigned long materialIndex = pScene->mMeshes[i]->mMaterialIndex;

        // 説明構造体
        MeshDX::Description desc{};
        desc.pVtx = vertices.data();
        desc.vtxSize = sizeof(MeshDX::Vertex);
        desc.vtxCount = static_cast<unsigned int>(vertices.size());
        desc.pIdx = indices.data();
        desc.idxSize = sizeof(unsigned long);
        desc.idxCount = static_cast<unsigned int>(indices.size());
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        meshes[i] = std::make_shared<MeshDX>(desc, materialIndex);
    }

    return meshes;
}

// マテリアル作成
std::vector<std::shared_ptr<MaterialDX>>
AssimpModelLorder::MakeMaterial(
    const aiScene* ptr,
    const std::string& directory,
    ID3D11Device* device,
    TextureCache& textureCache)
{
    const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);
    std::vector<std::shared_ptr<MaterialDX>> materials;
    materials.resize(pScene->mNumMaterials);

    for (unsigned int i = 0; i < materials.size(); ++i)
    {
        DirectX::XMFLOAT4 diffuse;
        DirectX::XMFLOAT4 ambient;
        DirectX::XMFLOAT4 specular;

        aiColor3D col;
        float shininess = 0.0f;

        if (pScene->mMaterials[i]->Get(AI_MATKEY_COLOR_DIFFUSE, col) == AI_SUCCESS)
            diffuse = { col.r, col.g, col.b, 1.0f };
        else
            diffuse = { 1, 1, 1, 1 };

        if (pScene->mMaterials[i]->Get(AI_MATKEY_COLOR_AMBIENT, col) == AI_SUCCESS)
            ambient = { col.r, col.g, col.b, 1.0f };
        else
            ambient = { 0.3f, 0.3f, 0.3f, 1.0f };

        if (pScene->mMaterials[i]->Get(AI_MATKEY_COLOR_SPECULAR, col) == AI_SUCCESS)
            specular = { col.r, col.g, col.b, 0.0f };
        else
            specular = { 0, 0, 0, 0 };

        if (pScene->mMaterials[i]->Get(AI_MATKEY_SHININESS, shininess) == AI_SUCCESS)
            specular.w = shininess;

        // テクスチャ
        std::shared_ptr<Texture2D> texture = nullptr;
        aiString texPath;
        if (pScene->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
        {
            std::string texFile = texPath.C_Str();
            std::string full = directory + texFile;
            std::wstring wfull(full.begin(), full.end());
            texture = textureCache.Load(full, device, wfull);
        }

        materials[i] = std::make_shared<MaterialDX>(diffuse, ambient, specular, texture);
    }

    return materials;
}
