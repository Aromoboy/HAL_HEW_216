#include "AssimpModelLorder.h"

#include <DirectXMath.h>
#include <limits>
#include <memory>
#include <optional>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <system_error>
#include <algorithm>
#include <utility>
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
std::shared_ptr<Render::Model> AssimpModelLorder::Load(
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
    return std::make_shared<Render::Model>(meshes, materials, bounds);
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
        const aiMesh* mesh = pScene->mMeshes[i];
        std::vector<MeshDX::Vertex> vertices;
        std::vector<unsigned long> indices;

        // 頂点
        vertices.resize(mesh->mNumVertices);
        for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
        {
            aiVector3D pos = mesh->mVertices[j];
            aiVector3D uv = mesh->HasTextureCoords(0)
                    ? mesh->mTextureCoords[0][j]
                    : aiVector3D(0, 0, 0);

            MeshDX::Vertex v{};
            v.pos = DirectX::XMFLOAT3(pos.x, pos.y, pos.z);
            v.uv = DirectX::XMFLOAT2(uv.x, uv.y);

            vertices[j] = v;

            minBounds.x = std::min(minBounds.x, pos.x);
            minBounds.y = std::min(minBounds.y, pos.y);
            minBounds.z = std::min(minBounds.z, pos.z);
            maxBounds.x = std::max(maxBounds.x, pos.x);
            maxBounds.y = std::max(maxBounds.y, pos.y);
            maxBounds.z = std::max(maxBounds.z, pos.z);
            hasBounds = true;
        }

        // インデックス
        indices.resize(mesh->mNumFaces * 3);
        for (unsigned int j = 0; j < mesh->mNumFaces; ++j)
        {
            aiFace face = mesh->mFaces[j];
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

    const auto modelPathU8 = modelPath.u8string();
    const std::string modelPath8(modelPathU8.begin(), modelPathU8.end());

    std::shared_ptr<Texture2D> fallbackTexture;
    const auto getFallbackTexture = [&]() -> std::shared_ptr<Texture2D>
    {
        if (fallbackTexture)
        {
            return fallbackTexture;
        }

        const std::filesystem::path fallbackPath("Assets/Texture/Shadow.png");
        const auto fallbackPathU8 = fallbackPath.u8string();
        const std::string fallbackPath8(fallbackPathU8.begin(), fallbackPathU8.end());
        const auto fallbackKey = TextureCache::MakeFileKey(fallbackPath8);

        if (!fallbackKey.empty())
        {
            fallbackTexture = textureCache.Load(fallbackKey, device, fallbackPath.wstring());
        }

        if (!fallbackTexture)
        {
            static const uint8_t whitePixel[4] = { 255, 255, 255, 255 };
            const auto generatedKey = TextureCache::NormalizeKey("generated:fallback:white");
            fallbackTexture = textureCache.LoadFromMemory(generatedKey, device, whitePixel, sizeof(whitePixel), 1, 1);

            if (!fallbackTexture)
            {
                DebugMessage("[AssimpModelLorder] Failed to create fallback texture SRV for model '" + modelPath8 + "'. Materials may bind null SRVs.\n");
            }
        }

        return fallbackTexture;
    };

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

        auto loadTexture = [&](aiTextureType type, bool allowFallback) -> std::pair<std::shared_ptr<Texture2D>, bool>
        {
            aiString texPath;
            if (pScene->mMaterials[i]->GetTexture(type, 0, &texPath) != AI_SUCCESS)
            {
                if (allowFallback)
                {
                    auto fallback = getFallbackTexture();
                    return { fallback, fallback != nullptr };
                }
                return { nullptr, false };
            }

            std::shared_ptr<Texture2D> texture = nullptr;
            bool usedFallback = false;
            std::string texFile = texPath.C_Str();

            if (!texFile.empty() && texFile[0] == '*')
            {
                const int texIndex = std::atoi(texFile.c_str() + 1);
                if (texIndex >= 0 && static_cast<unsigned int>(texIndex) < pScene->mNumTextures)
                {
                    const aiTexture* embedded = pScene->mTextures[texIndex];
                    const uint8_t* texData = reinterpret_cast<const uint8_t*>(embedded->pcData);
                    const auto cacheKey = isFbx
                        ? TextureCache::MakeFbxEmbeddedKey(modelPath8, texIndex)
                        : TextureCache::NormalizeKey(modelPath8 + ":embedded:" + std::to_string(texIndex));

                    if (cacheKey.empty())
                    {
                        DebugMessage("[AssimpModelLorder] Failed to build cache key for embedded texture in '" + modelPath8 + "'. Skipping texture load.\n");
                        usedFallback = true;
                    }
                    else
                    {
                        const size_t byteSize = embedded->mHeight == 0
                            ? static_cast<size_t>(embedded->mWidth)
                            : static_cast<size_t>(embedded->mWidth) * static_cast<size_t>(embedded->mHeight) * sizeof(aiTexel);

                        if (embedded->mHeight == 0)
                        {
                            texture = textureCache.LoadFromMemory(cacheKey, device, texData, byteSize, 0, 0, "compressed");
                        }
                        else
                        {
                            texture = textureCache.LoadFromMemory(
                                cacheKey,
                                device,
                                texData,
                                byteSize,
                                embedded->mWidth,
                                embedded->mHeight);
                        }

                        if (!texture)
                        {
                            DebugMessage("[AssimpModelLorder] Texture load returned null for embedded index "
                                + std::to_string(texIndex)
                                + " (material " + std::to_string(i)
                                + ") in model '" + modelPath8 + "'. Using fallback.\n");
                            usedFallback = true;
                        }
                    }
                }
            }
            else
            {
                std::filesystem::path texturePath;
                try
                {
                    texturePath = (directory / texFile).lexically_normal();
                }
                catch (const std::filesystem::filesystem_error& e)
                {
                    DebugMessage(
                        "[AssimpModelLorder] Failed to normalize texture path '" + texFile + "' for model '" + modelPath8
                        + "': " + e.what() + ". Skipping texture and using fallback.\n");
                    auto fallback = allowFallback ? getFallbackTexture() : nullptr;
                    return { fallback, true };
                }
                catch (const std::system_error& e)
                {
                    DebugMessage(
                        "[AssimpModelLorder] System error while resolving texture path '" + texFile + "' for model '" + modelPath8
                        + "': " + e.what() + ". Skipping texture and using fallback.\n");
                    auto fallback = allowFallback ? getFallbackTexture() : nullptr;
                    return { fallback, true };
                }

                const auto texturePathU8 = texturePath.u8string();
                const std::string texturePath8(texturePathU8.begin(), texturePathU8.end());
                const auto cacheKey = TextureCache::MakeFileKey(texturePath8);
                if (cacheKey.empty())
                {
                    DebugMessage("[AssimpModelLorder] Failed to build cache key for texture path '" + texturePath8 + "'. Skipping texture load.\n");
                    usedFallback = true;
                }
                else
                {
                    const std::wstring wfull = texturePath.wstring();
                    texture = textureCache.Load(cacheKey, device, wfull);

                    if (!texture)
                    {
                        DebugMessage("[AssimpModelLorder] Texture load returned null for path '" + texturePath8 + "' (material "
                            + std::to_string(i) + ") in model '" + modelPath8 + "'. Using fallback.\n");
                        usedFallback = true;
                    }
                }
            }

            if (!texture && allowFallback)
            {
                texture = getFallbackTexture();
                usedFallback = true;
            }

            return { texture, usedFallback };
        };

        auto [diffuseTexture, diffuseUsedFallback] = loadTexture(aiTextureType_DIFFUSE, true);

        const bool materialUsedFallback = diffuseUsedFallback || !diffuseTexture;

        if (!diffuseTexture && materialUsedFallback)
        {
            diffuseTexture = getFallbackTexture();
        }

        materials[i] = std::make_shared<MaterialDX>(
            diffuse,
            ambient,
            specular,
            diffuseTexture);
    }

    if (materials.empty())
    {
        auto fallback = getFallbackTexture();
        materials.push_back(std::make_shared<MaterialDX>(
            DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
            DirectX::XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f),
            DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f),
            fallback));
    }

    return materials;
}
