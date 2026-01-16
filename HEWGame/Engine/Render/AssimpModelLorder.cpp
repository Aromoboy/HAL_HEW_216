#include "AssimpModelLorder.h"

#include <DirectXMath.h>
#include <algorithm>
#include <cctype>
#include <limits>
#include <filesystem>
#include <memory>
#include <optional>
#include <chrono>
#include <future>
#include <cstdlib>
#include <cctype>
#include <string>
#include <string_view>
#include <sstream>
#include <vector>
#include <thread>
#include <unordered_map>
#include "Debug.h"
#include "TextureCache.h"
#include "TextureDX.h"
#include "ModelDX.h"
#include "MeshDX.h"
#include "MaterialDX.h"
#include "SweetsPrototypeManifest.h"
#include "ModelCache.h"

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

    std::string NormalizeSeparators(std::string value)
    {
        for (char& c : value)
        {
            if (c == '\\')
            {
                c = '/';
            }
        }
        return value;
    }

    std::string NormalizeForKey(std::string value)
    {
        value = NormalizeSeparators(std::move(value));
        for (char& c : value)
        {
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
        case SweetsType::Banana:
            return "Assets/Texture/banana.png";
        case SweetsType::Cream:
            return "Assets/Texture/Cream.png";

        case SweetsType::Kiwi :
            return "Assets/Texture/kiwi.png";
        case SweetsType::Mikan:
            return "Assets/Texture/mikan.png";
        case SweetsType::Muscat:
            return "Assets/Texture/muscat.png";
        case SweetsType::Melon:
            return "Assets/Texture/melon.png";
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
        if (contains("cream"))
        {
            return "Assets/Texture/Cream.png";
        }
        if (contains("banana"))
        {
            return "Assets/Texture/banana.png";
        }

        //01/09追加分
        if (contains("kiwi"))
        {
            return "Assets/Texture/kiwi.png";
        }
        if (contains("mikan"))
        {
            return "Assets/Texture/mikan.png";
        }
        if (contains("muscat"))
        {
            return "Assets/Texture/muscat.png";
        }
        if (contains("melon"))
        {
            return "Assets/Texture/melon.png";
        }
        return std::nullopt;
    }

    std::unordered_map<std::string, std::string> BuildTextureAliasMap()
    {
        std::unordered_map<std::string, std::string> aliases;

        const auto addAlias = [&aliases](const std::string& alias, const std::string& target)
        {
            aliases.emplace(NormalizeForKey(alias), target);
        };

        addAlias("パイナップル.png", "pineapple.png");
        addAlias("バナナ.png", "banana.png");
        addAlias("ブルーベリー.png", "blueberry.png");
        addAlias("ストロベリー.png", "strawberry.png");
        addAlias("いちご.png", "strawberry.png");
        addAlias("キウイ.png", "kiwi.png");
        addAlias("みかん.png", "mikan.png");
        addAlias("マスカット.png", "muscat.png");
        addAlias("メロン.png", "melon.png");
        addAlias("クリーム.png", "cream.png");

        return aliases;
    }

    std::unordered_map<std::string, std::string> BuildModelTextureOverrideMap()
    {
        std::unordered_map<std::string, std::string> overrides;

        const auto addOverride = [&overrides](const std::string& model, const std::string& texture)
        {
            overrides.emplace(NormalizeForKey(model), texture);
        };

        addOverride("cake.fbx", "Assets/Texture/cake.png");
        addOverride("assets/model/cake.fbx", "Assets/Texture/cake.png");
        addOverride("osara.fbx", "Assets/Texture/osara.png");
        addOverride("assets/model/osara.fbx", "Assets/Texture/osara.png");
        addOverride("base.fbx", "Assets/Texture/texture_diffuse.png");
        addOverride("assets/model/base.fbx", "Assets/Texture/texture_diffuse.png");

        return overrides;
    }

    std::optional<std::string> ResolveTextureAlias(const std::string& fileName)
    {
        static const std::unordered_map<std::string, std::string> kAliases = BuildTextureAliasMap();
        const std::string normalized = NormalizeForKey(fileName);

        const auto it = kAliases.find(normalized);
        if (it != kAliases.end())
        {
            return it->second;
        }

        return std::nullopt;
    }

    std::optional<std::string> ResolveTextureOverrideForModel(const std::string& normalizedModelPath)
    {
        static const std::unordered_map<std::string, std::string> kOverrides = BuildModelTextureOverrideMap();

        const auto directIt = kOverrides.find(NormalizeForKey(normalizedModelPath));
        if (directIt != kOverrides.end())
        {
            return directIt->second;
        }

        const std::filesystem::path asPath{ normalizedModelPath };
        const auto fileIt = kOverrides.find(NormalizeForKey(asPath.filename().string()));
        if (fileIt != kOverrides.end())
        {
            return fileIt->second;
        }

        return std::nullopt;
    }

    std::optional<std::string> ResolveFallbackTexture(const std::string& modelPath)
    {
        const std::string normalizedModelPath = NormalizePath(modelPath);

        if (auto override = ResolveTextureOverrideForModel(normalizedModelPath))
        {
            return override;
        }

        if (auto fromManifest = ResolveTextureFromManifest(normalizedModelPath))
        {
            if (std::filesystem::exists(*fromManifest))
            {
                return fromManifest;
            }
        }

        if (auto guessed = GuessTextureFromFilename(normalizedModelPath))
        {
            if (std::filesystem::exists(*guessed))
            {
                return guessed;
            }
        }

        return std::nullopt;
    }

    std::optional<std::string> ResolveFallbackTextureByName(const std::string& sanitizedFile, const std::string& modelPath)
    {
        if (auto alias = ResolveTextureAlias(sanitizedFile))
        {
            const std::string remapped = "Assets/Texture/" + *alias;
            if (std::filesystem::exists(remapped))
            {
                return remapped;
            }
        }

        const std::string normalizedFile = NormalizePath(sanitizedFile);
        if (auto guess = GuessTextureFromFilename(normalizedFile))
        {
            if (std::filesystem::exists(*guess))
            {
                return guess;
            }
        }

        return ResolveFallbackTexture(modelPath);
    }


    bool ContainsNonAscii(const std::string& value)
    {
        return std::any_of(value.begin(), value.end(), [](unsigned char c)
            {
                return c > 0x7F;
            });
    }

    bool EqualsIgnoreCase(const std::string& lhs, const std::string& rhs)
    {
        return lhs.size() == rhs.size()
            && std::equal(lhs.begin(), lhs.end(), rhs.begin(), [](char l, char r)
                {
                    return std::tolower(static_cast<unsigned char>(l))
                        == std::tolower(static_cast<unsigned char>(r));
                });
    }


    struct ImportResult
    {
        std::unique_ptr<Assimp::Importer> importer;
        const aiScene* scene = nullptr;
        std::string error;
    };

}

// モデル読み込み
std::shared_ptr<ModelDX> AssimpModelLorder::Load(
    const std::string& path,
    ID3D11Device* device,
    TextureCache& textureCache,
    const MaterialTuningSettings& materialTuningSettings)
{
    unsigned int flag = aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals | aiProcess_GenUVCoords;
    const std::string normalizedPath = NormalizePath(path);
    std::error_code absEc;
    std::string resolvedFullPath = std::filesystem::absolute(path, absEc).string();
    if (absEc)
    {
        resolvedFullPath = path;
    }

    std::error_code fileEc;
    const bool exists = std::filesystem::exists(resolvedFullPath, fileEc);
    const bool isFile = exists && std::filesystem::is_regular_file(resolvedFullPath, fileEc);
    const std::uintmax_t size = isFile ? std::filesystem::file_size(resolvedFullPath, fileEc) : 0;
    if (fileEc || !exists || !isFile || size == 0)
    {
        std::ostringstream oss;
        oss << "[AssimpModelLoader][ERROR] [CHECKPOINT B] インポート中止: ファイルが無いか空 path='" << path
            << "' resolved='" << resolvedFullPath << "'";
        if (fileEc)
        {
            oss << " ec='" << fileEc.message() << "'";
        }
        DebugLog::DebugMessage(oss.str());
        return nullptr;
    }

    DebugLog::DebugMessage("[AssimpModelLoader][CHECKPOINT A] ロード開始 path=" + path + " normalized=" + normalizedPath + " resolved=" + resolvedFullPath);
    auto future = std::async(std::launch::async, [path, flag]() -> ImportResult
        {
            ImportResult result;
            result.importer = std::make_unique<Assimp::Importer>();
            result.scene = result.importer->ReadFile(path.c_str(), flag);
            if (!result.scene)
            {
                result.error = result.importer->GetErrorString();
            }
            return result;
        });

    constexpr auto kImportTimeout = std::chrono::milliseconds(400);
    if (future.wait_for(kImportTimeout) == std::future_status::timeout)
    {
        DebugLog::DebugMessage(
            "[AssimpModelLoader][ERROR] [CHECKPOINT B] インポートがタイムアウト 取り消して終了 path=" + path);

        std::thread([f = std::move(future)]() mutable
            {
                if (f.valid())
                {
                    f.wait();
                }
            }).detach();
        return nullptr;
    }

    ImportResult importResult = future.get();
    const aiScene* pScene = importResult.scene;
    {
        std::ostringstream oss;
        oss << "[AssimpModelLoader][CHECKPOINT B] シーン=" << (pScene ? "ok" : "null")
            << " メッシュ=" << (pScene ? pScene->mNumMeshes : 0)
            << " マテリアル=" << (pScene ? pScene->mNumMaterials : 0);
        if (!pScene)
        {
            oss << " error='" << importResult.error << "'";
        }
        DebugLog::DebugMessage(oss.str());
    }
    if (!pScene)
    {
        return nullptr;
    }

    auto importer = std::move(importResult.importer);
    (void)importer;

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

    // ノード作成
    std::vector<NodeDX> nodes;

    MakeNode(pScene, nodes);

    // メッシュ作成
    AABB localBounds{};
    bool hasBounds = false;
    auto meshes = MakeMesh(pScene, localBounds, hasBounds, nodes);

    // マテリアル作成
    auto materials = MakeMaterial(pScene, directory, filename, device, textureCache, materialTuningSettings);
    const bool hasTextureOverride = ResolveTextureOverrideForModel(normalizedPath).has_value();
    for (unsigned int i = 0; i < meshes.size(); ++i)
    {
        aiMesh* mesh = pScene->mMeshes[i];
        if (!mesh || mesh->HasTextureCoords(0))
        {
            continue;
        }

        const auto meshPtr = meshes[i];
        if (!meshPtr)
        {
            continue;
        }

        const unsigned int materialId = meshPtr->GetMaterialID();
        if (materialId >= materials.size())
        {
            continue;
        }

        const auto& baseMaterial = materials[materialId];
        if (!baseMaterial || baseMaterial->GetTexture() == nullptr)
        {
            continue;
        }

        const bool hasOverrideTexture = hasTextureOverride && baseMaterial->GetTexture() != nullptr;
        if (hasOverrideTexture)
        {
            continue;
        }

        auto untexturedMaterial = std::make_shared<MaterialDX>(
            baseMaterial->GetDiffuse(),
            baseMaterial->GetAmbient(),
            baseMaterial->GetSpecular(),
            nullptr);
        ApplyMaterialTuning(*untexturedMaterial, "default", materialTuningSettings);
        materials.push_back(std::move(untexturedMaterial));
        meshPtr->SetMaterialID(static_cast<unsigned int>(materials.size() - 1));
    }

    // モデル生成
    std::optional<AABB> bounds = hasBounds ? std::optional<AABB>(localBounds) : std::nullopt;
    return std::make_shared<ModelDX>(meshes, materials, &materialTuningSettings, bounds);
}

// メッシュ作成
std::vector<std::shared_ptr<MeshDX>>
AssimpModelLorder::MakeMesh(const aiScene* ptr, AABB& outBounds, bool& hasBounds, std::vector<NodeDX> a_nodes)
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
        const bool hasTextureCoords = mesh->HasTextureCoords(0);

        if (!hasTextureCoords)
        {
            DebugLog::DebugMessage(
                std::string("[AssimpModelLoader][WARNING] UVなしメッシュ: ") + mesh->mName.C_Str());
        }

        float3 meshMin{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
        float3 meshMax{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
        bool meshHasBounds = false;

        DebugLog::DebugMessage("[AssimpModelLoader][CHECKPOINT F] メッシュ名=" + std::string(mesh->mName.C_Str()));
        aiMatrix4x4 nodeTransform;
        bool hasNodeTransform = false;
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
                nodeTransform = matchedNode->mTransformation;
                hasNodeTransform = true;
                std::ostringstream oss;
                oss << "[AssimpModelLoader][CHECKPOINT F] 対応ノード行列 " << mesh->mName.C_Str() << " : ";
                const aiMatrix4x4& m = matchedNode->mTransformation;
                oss << "[" << m.a1 << "," << m.a2 << "," << m.a3 << "," << m.a4 << ";"
                    << m.b1 << "," << m.b2 << "," << m.b3 << "," << m.b4 << ";"
                    << m.c1 << "," << m.c2 << "," << m.c3 << "," << m.c4 << ";"
                    << m.d1 << "," << m.d2 << "," << m.d3 << "," << m.d4 << "]";
                DebugLog::DebugMessage(oss.str() + "\n");
            }
            else
            {
                DebugLog::DebugMessage(std::string("[AssimpModelLoader][CHECKPOINT F] メッシュ ") + mesh->mName.C_Str() + " に対応するノードなし\n");
            }
        }

        const bool applyNodeTransform = hasNodeTransform && mesh->mNumBones == 0;
        if (applyNodeTransform)
        {
            DebugLog::DebugMessage("[Mesh] メッシュ '" + std::string(mesh->mName.C_Str()) + "' にノード行列を適用\n");
        }

        // 頂点
        vertices.resize(mesh->mNumVertices);
        DebugLog::DebugMessage("[AssimpModelLoader][CHECKPOINT E] メッシュ index=" + std::to_string(i)
            + " 頂点=" + std::to_string(mesh->mNumVertices)
            + " 面=" + std::to_string(mesh->mNumFaces)
            + " ボーン=" + std::string(mesh->HasBones() ? "yes" : "no") + "\n");

        for (unsigned int j = 0; j < vertices.size(); ++j)
        {
            aiVector3D pos = mesh->mVertices[j];
            aiVector3D normal = mesh->HasNormals()
                ? mesh->mNormals[j]
                : aiVector3D(0, 0, 0);
            aiVector3D uv = hasTextureCoords
                ? mesh->mTextureCoords[0][j]
                : aiVector3D(0, 0, 0);
            aiColor4D col = mesh->HasVertexColors(0)
                ? mesh->mColors[0][j]
                : aiColor4D(1, 1, 1, 1);

            if (applyNodeTransform)
            {
                pos = nodeTransform * pos;
            }

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

            meshMin.x = std::min(meshMin.x, pos.x);
            meshMin.y = std::min(meshMin.y, pos.y);
            meshMin.z = std::min(meshMin.z, pos.z);
            meshMax.x = std::max(meshMax.x, pos.x);
            meshMax.y = std::max(meshMax.y, pos.y);
            meshMax.z = std::max(meshMax.z, pos.z);
            meshHasBounds = true;
        }

        if (meshHasBounds)
        {
            std::ostringstream oss;
            oss << "[AssimpModelLoader][CHECKPOINT E] メッシュ境界 index=" << i
                << " min(" << meshMin.x << ',' << meshMin.y << ',' << meshMin.z
                << ") max(" << meshMax.x << ',' << meshMax.y << ',' << meshMax.z << ")";
            DebugLog::DebugMessage(oss.str());
        }

        std::vector<BoneDX> bones;
        // ボーン生成
        MakeVertexWeight(pScene, i, a_nodes, vertices, bones);

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
        desc.bones = bones;

        meshes[i] = std::make_shared<MeshDX>(desc, materialIndex);
    }

    if (hasBounds)
    {
        std::ostringstream oss;
        oss << "[AssimpModelLoader][CHECKPOINT E] 全体境界 min(" << minBounds.x << "," << minBounds.y << "," << minBounds.z
            << ") max(" << maxBounds.x << "," << maxBounds.y << "," << maxBounds.z << ")";
        DebugLog::DebugMessage(oss.str());
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
    TextureCache& textureCache,
    const MaterialTuningSettings& materialTuningSettings)
{
    const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);
    std::vector<std::shared_ptr<MaterialDX>> materials;
    materials.resize(pScene->mNumMaterials);
    const bool textureLoadingEnabled = Render::ModelCache::Instance().IsTextureLoadingEnabled();
    const std::string defaultMaterialKey = "default";

    if (!textureLoadingEnabled)
    {
        DebugLog::DebugMessage("[AssimpModelLoader] Texture loading disabled; materials will use null textures");
    }

    for (unsigned int i = 0; i < materials.size(); ++i)
    {
        DirectX::XMFLOAT4 diffuse;
        DirectX::XMFLOAT4 ambient;
        DirectX::XMFLOAT4 specular;

        const std::string modelPathForFallback = NormalizeSeparators(directory + modelFilename);

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
        if (textureLoadingEnabled)
        {
            const auto loadTexture = [&](const std::string& fullPath)
            {
                const std::string normalizedKey = NormalizeForKey(fullPath);
                std::wstring wfull(fullPath.begin(), fullPath.end());
                return textureCache.Load(normalizedKey, device, wfull);
            };

            if (auto overridePath = ResolveTextureOverrideForModel(modelPathForFallback))
            {
                DebugLog::DebugMessage(
                    std::string("[Material] モデル別オーバーライドテクスチャを優先適用: '") + *overridePath + "'");
                texture = loadTexture(*overridePath);
            }

            aiString texPath;
            if (!texture && pScene->mMaterials[i]->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
            {
                std::string texFile = NormalizeSeparators(texPath.C_Str());
                const std::string originalTexPath = texFile;

                std::string textureDirectory = NormalizeSeparators(directory);
                bool skipTextureLoading = false;

                const auto hasParentTraversal = [](const std::string& value)
                {
                    return value.find("..\\") != std::string::npos || value.find("../") != std::string::npos;
                };

                const bool isAbsoluteWindowsPath = texFile.size() >= 3
                    && std::isalpha(static_cast<unsigned char>(texFile[0]))
                    && texFile[1] == ':'
                    && (texFile[2] == '\\' || texFile[2] == '/');

                const bool needsSanitize =
                    std::filesystem::path(originalTexPath).is_absolute()
                    || isAbsoluteWindowsPath
                    || hasParentTraversal(originalTexPath);

                std::string fileName = std::filesystem::path(texFile).filename().string();
                std::string sanitizedFile = fileName;

                if (needsSanitize)
                {
                    const std::string sanitizedPath = "Assets/Texture/" + fileName;

                    DebugLog::DebugMessage(
                        std::string("[Material] テクスチャパスを正規化 '") + originalTexPath +
                        "' -> '" + sanitizedPath + "'");

                    if (std::filesystem::exists(sanitizedPath))
                    {
                        texFile = fileName;
                        textureDirectory = "Assets/Texture/";
                        sanitizedFile = fileName;
                    }
                    else if (auto alias = ResolveTextureAlias(fileName))
                    {
                        const std::string remappedPath = "Assets/Texture/" + *alias;
                        if (std::filesystem::exists(remappedPath))
                        {
                            texFile = *alias;
                            textureDirectory = "Assets/Texture/";
                            sanitizedFile = *alias;
                            DebugLog::DebugMessage(
                                std::string("[Material] テクスチャ '") + fileName + "' を '" + *alias + "' にマッピング");
                        }
                        else
                        {
                            DebugLog::DebugMessage(
                                std::string("[AssimpModelLoader][WARNING] テクスチャエイリアス '") +
                                remappedPath + "' が見つからないためスキップ");
                            skipTextureLoading = true;
                        }
                    }
                    else
                    {
                        DebugLog::DebugMessage(
                            std::string("[AssimpModelLoader][WARNING] 正規化後のテクスチャパスが存在しない: '") +
                            sanitizedPath + "' テクスチャをスキップ");
                        skipTextureLoading = true;
                    }
                }

                const std::string cacheKey = NormalizeForKey(textureDirectory + sanitizedFile);

                if (isAbsoluteWindowsPath
                    && (EqualsIgnoreCase(fileName, "texture.png") || EqualsIgnoreCase(fileName, "texture2.png")))
                {
                    texFile = fileName;
                    textureDirectory = "Assets\\Texture\\";
                }

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
                    const std::filesystem::path combinedPath = std::filesystem::path(textureDirectory) / texFile;
                    std::string full = NormalizeSeparators(combinedPath.string());

                    aiString baseColorPath;
                    std::string baseColorStr;
                    if (pScene->mMaterials[i]->GetTexture(aiTextureType_BASE_COLOR, 0, &baseColorPath) == AI_SUCCESS)
                    {
                        baseColorStr = baseColorPath.C_Str();
                    }

                    DebugLog::DebugMessage(
                        std::string("[AssimpModelLoader][CHECKPOINT C] マテリアル=") + std::to_string(i)
                        + " diffuse='" + texFile
                        + "' baseColor='" + baseColorStr
                        + "' modelDir=" + directory
                        + " fullPath=" + full
                    );

                    // --- フォールバックとパス整形 ---
                    const bool hasNonAscii = ContainsNonAscii(texFile);

                    std::error_code existEc;
                    const bool textureExists = std::filesystem::exists(full, existEc);
                    bool skipTextureLoad = skipTextureLoading;

                    if (skipTextureLoad || hasNonAscii || !textureExists)
                    {
                        if (auto fallback = ResolveFallbackTextureByName(sanitizedFile, modelPathForFallback))
                        {
                            DebugLog::DebugMessage(
                                std::string("[Material] 無効なパス '") + texFile + "' へフォールバックテクスチャ '" + *fallback + "' を使用");
                            full = *fallback;
                            skipTextureLoad = false;
                        }
                        else
                        {
                            DebugLog::DebugMessage(
                                std::string("[Material][WARNING] フォールバックテクスチャが見つからない '") + texFile + "' テクスチャをスキップ");
                            skipTextureLoad = true;
                        }
                    }

                    // --- 実際のロード ---
                    if (!skipTextureLoad)
                    {
                        texture = loadTexture(full);
                    }

                    if (!texture && texFile == "パイナップル.png")
                    {
                        const std::string alt = "Assets\\Texture\\" + texFile;
                        texture = loadTexture(alt);
                    }
                }
            }
            if (!texture)
            {
                if (auto fallback = ResolveFallbackTexture(modelPathForFallback))
                {
                    DebugLog::DebugMessage(
                        std::string("[Material] マニフェスト/名前フォールバックテクスチャ '") + *fallback + "' を適用 material " + std::to_string(i));
                    std::wstring wfull(fallback->begin(), fallback->end());
                    texture = textureCache.Load(NormalizeForKey(*fallback), device, wfull);
                }
            }
        }

        auto material = std::make_shared<MaterialDX>(diffuse, ambient, specular, texture);
        ApplyMaterialTuning(*material, defaultMaterialKey, materialTuningSettings);
        materials[i] = std::move(material);
    }

    return materials;
}

// Node周り

void AssimpModelLorder::MakeNode(const aiScene* ptr, std::vector<NodeDX>& a_outNodes)
{

    // 再帰処理でAssimpのノード情報を読み取り
    using FuncRecurciveMakeNodes = std::function < NodeIndexDX(aiNode*, NodeIndexDX, DirectX::XMMATRIX) >;
    FuncRecurciveMakeNodes func = [&func, &a_outNodes, this](aiNode* assimpNode, NodeIndexDX parent, DirectX::XMMATRIX mat)
        {
            // assimp側で階層情報が細かく分割されている場合の処理
            std::string name = assimpNode->mName.data;
            DirectX::XMMATRIX localTransform = GetMatrixFromAssimpMatrix(assimpNode->mTransformation);
            if (name.find("$AssimpFbx") != std::string::npos)
            {
                // 分割前のノードに到達するまで、各行列を掛け合わせていく
                mat = localTransform * mat;
                return func(assimpNode->mChildren[0], parent, mat);
            }

            // Assimpのノード情報を格納
            NodeDX node;
            node.name = name;
            node.parent = parent;
            node.children.resize(assimpNode->mNumChildren);
            node.mat = localTransform * mat;

            // ノードの一覧に追加
            a_outNodes.push_back(node);
            NodeIndexDX nodeIndex = static_cast<NodeIndexDX>(a_outNodes.size() - 1);

            // 子要素の情報を追加
            for (UINT i = 0; i < assimpNode->mNumChildren; ++i)
            {
                a_outNodes[nodeIndex].children[i] =
                    func(assimpNode->mChildren[i], nodeIndex, DirectX::XMMatrixIdentity());
            }

            return nodeIndex;
        };

    // ノード作成
    const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);
    func(pScene->mRootNode, kNodeNone, DirectX::XMMatrixIdentity());

}

NodeIndexDX AssimpModelLorder::FindNode(const char* name, const std::vector<NodeDX>& nodes)
{
    // 構築済みのボーンノードから該当ノードを取得
    std::vector<NodeDX>::const_iterator it = std::find_if(nodes.begin(), nodes.end(),
        [name](const NodeDX& val) {
            return val.name == name;
        });
    if (it == nodes.end()) {
        return kNodeNone;
    }

    return static_cast<NodeIndexDX>(it - nodes.begin());
}

void AssimpModelLorder::MakeVertexWeight(const void* ptr, int a_meshIndex, std::vector<NodeDX>& a_nodes, AssimpModelLorder::Vertices& a_vertices, std::vector<BoneDX>& a_bones)
{
    const aiScene* pScene = reinterpret_cast<const aiScene*>(ptr);
    aiMesh* assimpMesh = pScene->mMeshes[a_meshIndex];
    if (assimpMesh->HasBones())
        MakeVertexWeightHasBone(assimpMesh, a_vertices, a_nodes, a_bones);
    else
        MakeVertexWeightFromNode(ptr, assimpMesh, a_vertices, a_nodes, a_bones);
}

void AssimpModelLorder::MakeVertexWeightFromNode(const void* scene, const void* ptr, AssimpModelLorder::Vertices& a_vertices, std::vector<NodeDX> a_nodes, std::vector<BoneDX>& a_bones)
{
    const aiScene* pScene = reinterpret_cast<const aiScene*>(scene);
    const aiMesh* assimpMesh = reinterpret_cast<const aiMesh*>(ptr);

    // 階層内のメッシュを探索
    NodeIndexDX nodeIndex = FindNode(assimpMesh->mName.data, a_nodes);
    if (nodeIndex == kNodeNone) { return; }

    // メッシュでない親ノードを再帰探索
    using FuncRecurciveFindNoMesh = std::function<int(int)>;
    FuncRecurciveFindNoMesh func = [&func, &a_nodes, pScene](NodeIndexDX parent) -> NodeIndexDX
        {
            if (parent == kNodeNone) { return kNodeNone; }

            // 現在探索中のノード名がメッシュの一覧と一致するか確認
            std::string name = a_nodes[parent].name;
            for (UINT i = 0; i < pScene->mNumMeshes; ++i)
            {
                if (name == pScene->mMeshes[i]->mName.data)
                {
                    return func(a_nodes[parent].parent);
                }
            }

            // 一致しないノードを変換元として扱う
            return parent;
        };

    // ボーン割り当て情報を作成
    BoneDX bone = {};
    bone.nodeIndex = func(a_nodes[nodeIndex].parent);
    if (bone.nodeIndex != kNodeNone) {
        // ノードに保存されている行列を逆行列として設定
        DirectX::XMStoreFloat4x4(&bone.invOffset, DirectX::XMMatrixInverse(nullptr, a_nodes[bone.nodeIndex].mat));
        // ボーンの情報を設定
        a_bones.resize(1);
        a_bones[0] = bone;

        // 頂点すべてを割り当てたボーンに100%依存とする
        for (auto vtxIt = a_vertices.begin(); vtxIt != a_vertices.end(); ++vtxIt)
            vtxIt->weight[0] = 1.0f;
    }
}

void AssimpModelLorder::MakeVertexWeightHasBone(const void* ptr, AssimpModelLorder::Vertices& a_vertices, std::vector<NodeDX> a_nodes, std::vector<BoneDX>& a_bones)
{
    const aiMesh* assimpMesh = reinterpret_cast<const aiMesh*>(ptr);
    DirectX::XMMATRIX mFlip =
        DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f);
    DirectX::XMMATRIX mScale =
        DirectX::XMMatrixScaling(1.f, 1.f, 1.f);

    // 読み取り用のウェイトデータ作成
    struct WeightPair
    {
        unsigned int idx;
        float weight;
    };
    std::vector<std::vector<WeightPair>> weights;
    weights.resize(a_vertices.size());


    // メッシュに割り当てられているボーン領域確保
    a_bones.resize(assimpMesh->mNumBones);
    for (auto boneIt = a_bones.begin(); boneIt != a_bones.end(); ++boneIt)
    {
        UINT boneIdx = static_cast<UINT>(boneIt - a_bones.begin());
        aiBone* assimpBone = assimpMesh->mBones[boneIdx];

        // 構築済みのノードから該当ノードのインデックスを取得
        boneIt->nodeIndex = FindNode(assimpBone->mName.data, a_nodes);
        if (boneIt->nodeIndex == kNodeNone)
        {
            continue;
        }

        // メッシュに割り当てられているボーンを、原点に戻す逆行列を設定
        DirectX::XMMATRIX invOffsetMatrix;

        invOffsetMatrix = GetMatrixFromAssimpMatrix(assimpBone->mOffsetMatrix);
        invOffsetMatrix = mFlip * invOffsetMatrix * mScale;

        DirectX::XMStoreFloat4x4(&boneIt->invOffset, invOffsetMatrix);

        // ウェイトの設定
        for (UINT i = 0; i < assimpBone->mNumWeights; ++i)
        {
            aiVertexWeight& weight = assimpBone->mWeights[i];
            weights[weight.mVertexId].push_back({ boneIdx, weight.mWeight });
        }
    }

    // 取得してきた頂点ウェイトを頂点データに書き込み
    for (int i = 0; i < weights.size(); ++i)
    {
        // プログラム側の割り当て数を超える場合の正規化処理
        if (weights[i].size() >= MAX_WEIGHT)
        {
            // 割合が高い順にソート
            std::sort(weights[i].begin(), weights[i].end(),
                [](WeightPair& a, WeightPair& b) {
                    return a.weight > b.weight;
                });
            // ウェイト数に合わせて正規化
            float total = 0.0f;
            for (int j = 0; j < MAX_WEIGHT; ++j)
                total += weights[i][j].weight;
            for (int j = 0; j < MAX_WEIGHT; ++j)
                weights[i][j].weight /= total;
        }

        // 正規化したデータを頂点データに割り当て
        for (int j = 0; j < weights[i].size() && j < MAX_WEIGHT; ++j)
        {
            a_vertices[i].index[j] = weights[i][j].idx;
            a_vertices[i].weight[j] = weights[i][j].weight;
        }
    }
}

DirectX::XMMATRIX AssimpModelLorder::GetMatrixFromAssimpMatrix(aiMatrix4x4 M)
{
    /*
* @brief assimp内の行列をXMMATRIX型に変換
* @param[in] M assimpの行列
* @return 変換後の行列
*/
    return DirectX::XMMatrixSet(
        M.a1, M.b1, M.c1, M.d1,
        M.a2, M.b2, M.c2, M.d2,
        M.a3, M.b3, M.c3, M.d3,
        M.a4, M.b4, M.c4, M.d4
    );

}
