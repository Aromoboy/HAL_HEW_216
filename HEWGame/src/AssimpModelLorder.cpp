#include "AssimpModelLorder.h"

#include <DirectXMath.h>
#include <algorithm>
#include <cctype>
#include <limits>
#include <filesystem>
#include <memory>
#include <optional>
#include <cstdlib>
#include <cctype>
#include <string>
#include <string_view>
#include <sstream>
#include <vector>
#include "Debug.h"
#include "TextureCache.h"
#include "TextureDX.h"
#include "ModelDX.h"
#include "MeshDX.h"
#include "MaterialDX.h"
#include "SweetsPrototypeManifest.h"

#if defined(_WIN32)
#    include <windows.h>
#endif

AssimpModelLorder::AssimpModelLorder() = default;

namespace
{
    std::string NormalizePath(std::string value)
    {
        for (char& c : value)
        {
            if (c == '\\')
            {
                c = '/';
            }
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return value;
    }

    bool EndsWithPath(const std::string& fullPath, const std::string& suffix)
    {
        if (fullPath.size() < suffix.size())
        {
            return false;
        }
        return fullPath.compare(fullPath.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    std::optional<std::string> MapSweetsTypeToTexture(SweetsSpace::SweetsType type)
    {
        using SweetsSpace::SweetsType;
        switch (type)
        {
        case SweetsType::Strawberry:
            return "Assets/Texture/strawberry.png";
        case SweetsType::Blueberry:
            return "Assets/Texture/blueberry.png";
        case SweetsType::Pineapple:
            return "Assets/Texture/pineapple.png";
        case SweetsType::Cake:
        case SweetsType::Pudding:
        case SweetsType::Sherbet:
        case SweetsType::Banana:
            return "Assets/Texture/cream.png";
        case SweetsType::Fairy:
        default:
            return std::nullopt;
        }
    }

    std::optional<std::string> ResolveTextureFromManifest(const std::string& normalizedModelPath)
    {
        for (const auto& entry : SweetsSpace::SweetsPrototypeManifest::Instance().Enumerate())
        {
            std::string candidate = NormalizePath(entry.descriptor.modelPath);
            if (candidate.empty())
            {
                continue;
            }

            if (EndsWithPath(normalizedModelPath, candidate))
            {
                return MapSweetsTypeToTexture(entry.type);
            }
        }

        return std::nullopt;
    }

    std::optional<std::string> GuessTextureFromFilename(const std::string& normalizedModelPath)
    {
        auto contains = [&](std::string_view token)
        {
            return normalizedModelPath.find(token) != std::string::npos;
        };

        if (contains("strawberry"))
        {
            return "Assets/Texture/strawberry.png";
        }
        if (contains("blueberry"))
        {
            return "Assets/Texture/blueberry.png";
        }
        if (contains("pineapple"))
        {
            return "Assets/Texture/pineapple.png";
        }
        if (contains("cake") || contains("cream") || contains("pudding") || contains("banana"))
        {
            return "Assets/Texture/cream.png";
        }

        return std::nullopt;
    }

    std::optional<std::string> ResolveFallbackTexture(const std::string& modelPath)
    {
        const std::string normalizedPath = NormalizePath(modelPath);

        if (auto mapped = ResolveTextureFromManifest(normalizedPath))
        {
            return mapped;
        }

        return GuessTextureFromFilename(normalizedPath);
    }
}

// モデル読み込み
std::shared_ptr<ModelDX> AssimpModelLorder::Load(
    const std::string& path,
    ID3D11Device* device,
    TextureCache& textureCache)
{
    Assimp::Importer importer;
    unsigned int flag = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals;
    const std::string normalizedPath = NormalizePath(path);
    std::error_code absEc;
    std::string resolvedFullPath = std::filesystem::absolute(path, absEc).string();
    if (absEc)
    {
        resolvedFullPath = path;
    }
    DebugMessage("[AssimpModelLoader][CHECKPOINT A] load start path=" + path + " normalized=" + normalizedPath + " resolved=" + resolvedFullPath);
    const aiScene* pScene = importer.ReadFile(path.c_str(), flag);
    {
        std::ostringstream oss;
        oss << "[AssimpModelLoader][CHECKPOINT B] scene=" << (pScene ? "ok" : "null")
            << " meshes=" << (pScene ? pScene->mNumMeshes : 0)
            << " materials=" << (pScene ? pScene->mNumMaterials : 0);
        if (!pScene)
        {
            oss << " error='" << importer.GetErrorString() << "'";
        }
        DebugMessage(oss.str());
    }
    if (!pScene)
        return nullptr;

    // ディレクトリ抽出
    std::string directory = path;
    for (auto& c : directory)
        if (c == '/')
            c = '\\';
    directory = directory.substr(0, directory.find_last_of('\\') + 1);

    std::string filename = path;
    const auto slashPos = filename.find_last_of("/\\");
    if (slashPos != std::string::npos)
    {
        filename = filename.substr(slashPos + 1);
    }

    // メッシュ作成
    AABB localBounds{};
    bool hasBounds = false;
    auto meshes = MakeMesh(pScene, localBounds, hasBounds);

    // マテリアル作成
    auto materials = MakeMaterial(pScene, directory, filename, device, textureCache);

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
        aiMesh* mesh = pScene->mMeshes[i];
        std::vector<MeshDX::Vertex> vertices;
        std::vector<unsigned long> indices;

        DebugMessage("[AssimpModelLoader][CHECKPOINT F] mesh name=" + std::string(mesh->mName.C_Str()));
        {
            const auto findNode = [&](const auto& self, const aiNode* node, const aiString& name) -> const aiNode*
            {
                if (!node)
                {
                    return nullptr;
                }
                if (name == node->mName)
                {
                    return node;
                }
                for (unsigned int childIdx = 0; childIdx < node->mNumChildren; ++childIdx)
                {
                    if (const aiNode* found = self(self, node->mChildren[childIdx], name))
                    {
                        return found;
                    }
                }
                return nullptr;
            };

            const aiNode* matchedNode = findNode(findNode, pScene->mRootNode, mesh->mName);
            if (matchedNode)
            {
                std::ostringstream oss;
                oss << "[AssimpModelLoader][CHECKPOINT F] node transform for " << mesh->mName.C_Str() << " : ";
                const aiMatrix4x4& m = matchedNode->mTransformation;
                oss << "[" << m.a1 << "," << m.a2 << "," << m.a3 << "," << m.a4 << ";"
                    << m.b1 << "," << m.b2 << "," << m.b3 << "," << m.b4 << ";"
                    << m.c1 << "," << m.c2 << "," << m.c3 << "," << m.c4 << ";"
                    << m.d1 << "," << m.d2 << "," << m.d3 << "," << m.d4 << "]";
                DebugMessage(oss.str());
            }
            else
            {
                DebugMessage("[AssimpModelLoader][CHECKPOINT F] no node found for mesh " + std::string(mesh->mName.C_Str()));
            }
        }

        // 頂点
        vertices.resize(mesh->mNumVertices);
        DebugMessage("[AssimpModelLoader][CHECKPOINT E] mesh index=" + std::to_string(i)
            + " vertices=" + std::to_string(mesh->mNumVertices)
            + " faces=" + std::to_string(mesh->mNumFaces)
            + " bones=" + std::string(mesh->HasBones() ? "yes" : "no"));

        for (unsigned int j = 0; j < vertices.size(); ++j)
        {
            aiVector3D pos = mesh->mVertices[j];
            aiVector3D normal = mesh->HasNormals()
                ? mesh->mNormals[j]
                : aiVector3D(0, 0, 0);
            aiVector3D uv = mesh->HasTextureCoords(0)
                ? mesh->mTextureCoords[0][j]
                : aiVector3D(0, 0, 0);
            aiColor4D col = mesh->HasVertexColors(0)
                ? mesh->mColors[0][j]
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
        unsigned long materialIndex = mesh->mMaterialIndex;

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
        std::ostringstream oss;
        oss << "[AssimpModelLoader][CHECKPOINT E] bounds min(" << minBounds.x << "," << minBounds.y << "," << minBounds.z
            << ") max(" << maxBounds.x << "," << maxBounds.y << "," << maxBounds.z << ")";
        DebugMessage(oss.str());
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
    const std::string& directory,
    const std::string& modelFilename,
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
            for (auto& c : texFile)
            {
                if (c == '/')
                {
                    c = '\\';
                }
            }

            std::string textureDirectory = directory;

            const size_t lastSeparator = texFile.find_last_of("\\/");
            const std::string fileName = lastSeparator == std::string::npos
                ? texFile
                : texFile.substr(lastSeparator + 1);

            const bool isAbsoluteWindowsPath = texFile.size() >= 3
                && std::isalpha(static_cast<unsigned char>(texFile[0]))
                && texFile[1] == ':'
                && (texFile[2] == '\\' || texFile[2] == '/');

            const auto equalsIgnoreCase = [](const std::string& lhs, const std::string& rhs)
            {
                return lhs.size() == rhs.size()
                    && std::equal(lhs.begin(), lhs.end(), rhs.begin(), [](char l, char r)
                        {
                            return std::tolower(static_cast<unsigned char>(l))
                                == std::tolower(static_cast<unsigned char>(r));
                        });
            };

            if (isAbsoluteWindowsPath
                && (equalsIgnoreCase(fileName, "texture.png") || equalsIgnoreCase(fileName, "texture2.png")))
            {
                texFile = fileName;
                textureDirectory = "Assets\\Texture\\";
            }

            std::string cacheKey = textureDirectory + texFile;

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
                    texture = textureCache.LoadFromMemory(cacheKey, device, texData, byteSize, embedded->mWidth, embedded->mHeight);
                }
            }
            else
            {
                const auto loadTexture = [&](const std::string& fullPath)
                {
                    std::wstring wfull(fullPath.begin(), fullPath.end());
                    return textureCache.Load(fullPath, device, wfull);
                };

                std::string full = textureDirectory + texFile;
                aiString baseColorPath;
                std::string baseColorStr;
                if (pScene->mMaterials[i]->GetTexture(aiTextureType_BASE_COLOR, 0, &baseColorPath) == AI_SUCCESS)
                {
                    baseColorStr = baseColorPath.C_Str();
                }
                DebugMessage("[AssimpModelLoader][CHECKPOINT C] material=" + std::to_string(i)
                    + " diffuse='" + texFile + "' baseColor='" + baseColorStr + "' modelDir=" + directory + " fullPath=" + full);
                texture = loadTexture(full);

                if (!texture && texFile == "パイナップル.png")
                {
                    const std::string alt = "Assets\\Texture\\" + texFile;
                    texture = loadTexture(alt);
                }
            }
        }
        if (!texture)
        {
            const std::string modelPathForFallback = directory + modelFilename;
            if (auto fallback = ResolveFallbackTexture(modelPathForFallback))
            {
                std::wstring wfull(fallback->begin(), fallback->end());
                texture = textureCache.Load(*fallback, device, wfull);
            }
        }

        materials[i] = std::make_shared<MaterialDX>(diffuse, ambient, specular, texture);
    }

    return materials;
}
