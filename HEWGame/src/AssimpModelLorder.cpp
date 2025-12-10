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
#include <cctype>
#include <utility>
#include <vector>
#include <filesystem>
#include <codecvt>
#include "Debug.h"
#include "TextureCache.h"
#include "TextureDX.h"
#include "ModelDX.h"
#include "MeshDX.h"
#include "MaterialDX.h"

#if defined(_WIN32)
#    include <windows.h>
#endif

namespace
{
    struct ResolvedTexturePath
    {
        std::string cacheKey;
        std::wstring fullPath;
    };

    std::string TrimTexturePath(const std::string& path)
    {
        const auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };

        auto begin = path.begin();
        auto end = path.end();

        while (begin != end && isSpace(static_cast<unsigned char>(*begin)))
        {
            ++begin;
        }

        while (end != begin && isSpace(static_cast<unsigned char>(*(end - 1))))
        {
            --end;
        }

        return std::string(begin, end);
    }

    std::optional<ResolvedTexturePath> ResolveTexturePath(
        const aiString& texPath,
        const aiScene* /*scene*/,
        const std::string& modelDir,
        const std::string& modelPath8)
    {
        std::string texturePath8 = TrimTexturePath(texPath.C_Str());

        std::replace(texturePath8.begin(), texturePath8.end(), '\\', '/');

        if (texturePath8.empty())
        {
            DebugMessage(
                "[AssimpModelLorder] Texture path is empty in material from model '" + modelPath8
                + "'. Skipping texture load.\n");
            return std::nullopt;
        }

        const auto isAbsolute = [&texturePath8]()
        {
            if (texturePath8.size() >= 2 && std::isalpha(static_cast<unsigned char>(texturePath8[0])) && texturePath8[1] == ':')
            {
                return true;
            }

            return !texturePath8.empty() && texturePath8[0] == '/';
        }();

        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

        try
        {
            std::filesystem::path fullPath;

            if (isAbsolute)
            {
                const std::wstring wide = converter.from_bytes(texturePath8);
                fullPath = std::filesystem::path(wide).lexically_normal();
            }
            else
            {
                std::filesystem::path base(modelDir);
                std::filesystem::path relative(std::u8string(texturePath8.begin(), texturePath8.end()));
                fullPath = (base / relative).lexically_normal();
            }

            const auto filenameU8 = fullPath.filename().u8string();
            std::string cacheKey(filenameU8.begin(), filenameU8.end());
            std::transform(cacheKey.begin(), cacheKey.end(), cacheKey.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            return ResolvedTexturePath{ cacheKey, fullPath.wstring() };
        }
        catch (const std::range_error& e)
        {
            DebugMessage(
                "[AssimpModelLorder] Failed to convert texture path '" + texturePath8 + "' for model '" + modelPath8
                + "' to UTF-16: " + e.what() + ". Using fallback texture if available.\n");
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            DebugMessage(
                "[AssimpModelLorder] Filesystem error while resolving texture path '" + texturePath8 + "' for model '" + modelPath8
                + "': " + e.what() + ". Using fallback texture if available.\n");
        }
        catch (const std::exception& e)
        {
            DebugMessage(
                "[AssimpModelLorder] Unexpected error while resolving texture path '" + texturePath8 + "' for model '" + modelPath8
                + "': " + e.what() + ". Using fallback texture if available.\n");
        }

        return std::nullopt;
    }
}

AssimpModelLorder::AssimpModelLorder() = default;

// モデル読み込み
std::shared_ptr<Render::Model> AssimpModelLorder::Load(
    const std::string& path,
    ID3D11Device* device,
    TextureCache& textureCache)
{
    Assimp::Importer importer;
    unsigned int flag = aiProcess_Triangulate | aiProcess_FlipUVs;
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
        desc.vtxSize = MeshDX::VertexStride;
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
    const auto modelDirU8 = directory.u8string();
    const std::string modelDir8(modelDirU8.begin(), modelDirU8.end());

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
            DebugMessage(
                "[AssimpModelLorder] Fallback texture '" + fallbackPath8
                + "' could not be loaded; materials will bind null SRVs when missing textures.\n");
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
                DebugMessage(
                    "[AssimpModelLorder] Material " + std::to_string(i)
                    + " in model '" + modelPath8 + "' has no texture for type "
                    + std::to_string(static_cast<int>(type)) + ".\n");

                if (allowFallback)
                {
                    auto fallback = getFallbackTexture();
                    if (!fallback)
                    {
                        DebugMessage("[AssimpModelLorder] No fallback texture available; SRV will be null.\n");
                    }
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
                const auto resolved = ResolveTexturePath(texPath, pScene, modelDir8, modelPath8);
                if (!resolved)
                {
                    auto fallback = allowFallback ? getFallbackTexture() : nullptr;
                    return { fallback, true };
                }

                const auto& cacheKey = resolved->cacheKey;
                if (cacheKey.empty())
                {
                    DebugMessage(
                        "[AssimpModelLorder] Failed to build cache key for texture path '" + texFile + "' in model '" + modelPath8
                        + "'. Skipping texture load.\n");
                    usedFallback = true;
                }
                else
                {
                    texture = textureCache.Load(cacheKey, device, resolved->fullPath);

                    if (!texture)
                    {
                        DebugMessage("[AssimpModelLorder] Texture load returned null for path '" + texFile + "' (material "
                            + std::to_string(i) + ") in model '" + modelPath8 + "'. Using fallback.\n");
                        usedFallback = true;
                    }
                }
            }

            if (!texture && allowFallback)
            {
                texture = getFallbackTexture();
                usedFallback = texture != nullptr;

                if (usedFallback)
                {
                    DebugMessage(
                        "[AssimpModelLorder] Using fallback texture for material " + std::to_string(i)
                        + " in model '" + modelPath8 + "'.\n");
                }
                else
                {
                    DebugMessage(
                        "[AssimpModelLorder] Fallback requested for material " + std::to_string(i)
                        + " in model '" + modelPath8 + "', but no fallback texture is available. SRV will be null.\n");
                }
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
