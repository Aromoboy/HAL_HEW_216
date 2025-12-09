#include "AssimpModelLorder.h"

#include <DirectXMath.h>
#include <limits>
#include <memory>
#include <optional>
#include <cstdlib>
#include <string>
#include <vector>
#include <filesystem>
#include "Debug.h"
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
    unsigned int flag = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals;
    const aiScene* pScene = importer.ReadFile(path.c_str(), flag);
    if (!pScene)
        return nullptr;

    const std::filesystem::path modelPath(path);

    std::string pathWithoutExtension = path;
    for (auto& c : pathWithoutExtension)
        if (c == '\\')
            c = '/';
    const auto extPos = pathWithoutExtension.find_last_of('.');
    if (extPos != std::string::npos)
    {
        pathWithoutExtension = pathWithoutExtension.substr(0, extPos);
    }

    // メッシュ作成
    AABB localBounds{};
    bool hasBounds = false;
    auto meshes = MakeMesh(pScene, localBounds, hasBounds);

    // マテリアル作成
    auto materials = MakeMaterial(pScene, modelPath, device, textureCache);

    // モデル生成
    std::optional<AABB> bounds = hasBounds ? std::optional<AABB>(localBounds) : std::nullopt;
    return std::make_shared<ModelDX>(meshes, materials, bounds);
}

// メッシュ作成
std::vector<std::shared_ptr<MeshDX>>
AssimpModelLorder::MakeMesh(const aiScene* ptr, AABB& outBounds, bool& hasBounds)
{
    const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);
    std::vector<std::shared_ptr<MeshDX>> meshes;
    meshes.resize(pScene->mNumMeshes);

    float3 minBounds{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
    float3 maxBounds{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
    hasBounds = false;

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

            DirectX::XMVECTOR normalVec = DirectX::XMVectorSet(normal.x, normal.y, normal.z, 0.0f);
            const auto normalLenSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(normalVec));
            if (normalLenSq > 0.0f)
            {
                normalVec = DirectX::XMVector3Normalize(normalVec);
            }
            else
            {
                normalVec = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            }

            DirectX::XMFLOAT3 normalFloat;
            DirectX::XMStoreFloat3(&normalFloat, normalVec);

            vertices[j] = {
                DirectX::XMFLOAT3(pos.x, pos.y, pos.z),
                normalFloat,
                DirectX::XMFLOAT2(uv.x, uv.y),
                DirectX::XMFLOAT4(col.r, col.g, col.b, col.a)
            };

            minBounds.x = std::min(minBounds.x, pos.x);
            minBounds.y = std::min(minBounds.y, pos.y);
            minBounds.z = std::min(minBounds.z, pos.z);
            maxBounds.x = std::max(maxBounds.x, pos.x);
            maxBounds.y = std::max(maxBounds.y, pos.y);
            maxBounds.z = std::max(maxBounds.z, pos.z);
            hasBounds = true;
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

    if (hasBounds)
    {
        outBounds = AABB(maxBounds, minBounds);
    }

    return meshes;
}

// マテリアル作成
std::vector<std::shared_ptr<MaterialDX>>
AssimpModelLorder::MakeMaterial(
    const aiScene* ptr,
    const std::filesystem::path& modelPath,
    ID3D11Device* device,
    TextureCache& textureCache)
{
    const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);
    std::vector<std::shared_ptr<MaterialDX>> materials;
    materials.resize(pScene->mNumMaterials);

    const auto directory = modelPath.parent_path();
    const auto modelExtensionU8 = modelPath.extension().u8string();
    const std::string modelExtension8(modelExtensionU8.begin(), modelExtensionU8.end());
    const auto modelExtension = TextureCache::NormalizeKey(modelExtension8);
    const bool isFbx = modelExtension == ".fbx";

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

            if (!texFile.empty() && texFile[0] == '*')
            {
                const int texIndex = std::atoi(texFile.c_str() + 1);
                if (texIndex >= 0 && static_cast<unsigned int>(texIndex) < pScene->mNumTextures)
                {
                    const aiTexture* embedded = pScene->mTextures[texIndex];
                    const uint8_t* texData = reinterpret_cast<const uint8_t*>(embedded->pcData);
                    const size_t byteSize = embedded->mHeight == 0
                        ? static_cast<size_t>(embedded->mWidth)
                        : static_cast<size_t>(embedded->mWidth) * static_cast<size_t>(embedded->mHeight) * sizeof(aiTexel);
                    const auto modelPathU8 = modelPath.u8string();
                    const std::string modelPath8(modelPathU8.begin(), modelPathU8.end());
                    const auto cacheKey = isFbx
                        ? TextureCache::MakeFbxEmbeddedKey(modelPath8, texIndex)
                        : TextureCache::NormalizeKey(modelPath8 + ":embedded:" + std::to_string(texIndex));
                    texture = textureCache.LoadFromMemory(cacheKey, device, texData, byteSize, embedded->mWidth, embedded->mHeight);
                }
            }
            else
            {
                std::filesystem::path texturePath = directory / texFile;
                texturePath = texturePath.lexically_normal();
                const auto texturePathU8 = texturePath.u8string();
                const std::string texturePath8(texturePathU8.begin(), texturePathU8.end());
                const auto cacheKey = TextureCache::MakeFileKey(texturePath8);
                const std::wstring wfull = texturePath.wstring();
                texture = textureCache.Load(cacheKey, device, wfull);
            }
        }

        materials[i] = std::make_shared<MaterialDX>(diffuse, ambient, specular, texture);
    }

    return materials;
}
