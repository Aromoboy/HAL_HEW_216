#include "StageLayoutLoader.h"

#include "BoxColliderComponent.h"
#include "ColliderRegistry.h"
#include "ModelCache.h"
#include "Physics/ColliderBuilder.h"
#include "SceneGameConfigLoader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#if defined(_WIN32)
#       ifndef NOMINMAX
#               define NOMINMAX
#       endif
#       include <windows.h>
#endif

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
    void DebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    void DebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

    constexpr bool kEnableStageModelVerboseLogging = false;

    std::string NormalizeColliderKey(const std::string& key)
    {
        std::string normalized;
        normalized.reserve(key.size());
        for (unsigned char ch : key)
        {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
        }
        return normalized;
    }

    std::string BuildModelKey(const std::string& modelPath)
    {
        std::string key = modelPath;
        try
        {
            const std::filesystem::path path(modelPath);
            key = path.stem().string();
        }
        catch (...)
        {
            // そのまま modelPath を使う
        }

        return NormalizeColliderKey(key);
    }

    std::optional<SceneGameConfig::StageColliderPreset> ResolveStageColliderPreset(
        const SceneGameConfig::StageColliderConfig& config,
        const std::string& explicitPreset,
        const std::string& modelPath)
    {
        const std::string requested = NormalizeColliderKey(explicitPreset);
        if (!requested.empty())
        {
            auto it = config.presetsById.find(requested);
            if (it != config.presetsById.end())
            {
                return it->second;
            }
        }

        const std::string modelKey = BuildModelKey(modelPath);
        auto byModel = config.presetsByModelKey.find(modelKey);
        if (byModel != config.presetsByModelKey.end())
        {
            return byModel->second;
        }

        auto fallbackPreset = config.presetsById.find(modelKey);
        if (fallbackPreset != config.presetsById.end())
        {
            return fallbackPreset->second;
        }

        return std::nullopt;
    }

    bool TryParseFloat(const std::string& token, float& value, const std::string& logPrefix, const char* fieldName)
    {
        if (token.empty())
        {
            DebugTrace(logPrefix + "StageLayout.csv empty token for " + fieldName + "\n");
            return false;
        }

        try
        {
            value = std::stof(token);
            return true;
        }
        catch (const std::exception& ex)
        {
            DebugTrace(logPrefix + "StageLayout.csv invalid token for " + fieldName + ": " + ex.what() + "\n");
            return false;
        }
    }

    bool AreAllTokensEmpty(const std::vector<std::string>& values)
    {
        return std::all_of(values.begin(), values.end(), [](const std::string& token)
            {
                return token.empty();
            });
    }

}

StageLayoutLoadResult StageLayoutLoader::LoadFromCsv(const StageLayoutLoadRequest& request)
{
    StageLayoutLoadResult result;
    const std::string logPrefix = request.logPrefix.empty() ? std::string("[StageLayoutLoader] ") : request.logPrefix;

    std::ifstream file(request.csvPath);
    if (!file.is_open())
    {
        DebugTrace(logPrefix + "StageLayout.csvを開けませんでした\n");
        return result;
    }

    DebugTrace(logPrefix + "CSVファイルを開きました: " + request.csvPath + "\n");

    SceneGameConfig::StageColliderConfig stageColliderConfig{};
    if (request.enablePhysics)
    {
        const bool registryLoaded = Physics::ColliderRegistry::Instance().LoadFromFile(request.gameplayConfigPath);
        if (!registryLoaded)
        {
            DebugTrace(logPrefix + "Stage collider registry missing; falling back to cached definitions if any.\n");
        }
        stageColliderConfig = SceneGameConfig::LoadStageColliderConfig(request.gameplayConfigPath);
    }

    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> values;

        while (std::getline(ss, token, ','))
        {
            values.push_back(token);
        }

        if (values.size() < 15)
        {
            continue;
        }

        if (AreAllTokensEmpty(values))
        {
            continue;
        }

        StageObject obj;
        obj.modelPath = values[0];
        if (obj.modelPath.empty())
        {
            DebugTrace(logPrefix + "StageLayout.csv missing model path; skipped line.\n");
            continue;
        }

        if (!TryParseFloat(values[1], obj.transform.position.x, logPrefix, "position.x")
            || !TryParseFloat(values[2], obj.transform.position.y, logPrefix, "position.y")
            || !TryParseFloat(values[3], obj.transform.position.z, logPrefix, "position.z")
            || !TryParseFloat(values[4], obj.transform.rotation.x, logPrefix, "rotation.x")
            || !TryParseFloat(values[5], obj.transform.rotation.y, logPrefix, "rotation.y")
            || !TryParseFloat(values[6], obj.transform.rotation.z, logPrefix, "rotation.z")
            || !TryParseFloat(values[7], obj.transform.scale.x, logPrefix, "scale.x")
            || !TryParseFloat(values[8], obj.transform.scale.y, logPrefix, "scale.y")
            || !TryParseFloat(values[9], obj.transform.scale.z, logPrefix, "scale.z"))
        {
            continue;
        }

        std::string spinValue = values[10];
        std::transform(spinValue.begin(), spinValue.end(), spinValue.begin(), ::tolower);

        obj.spinEnabled = (spinValue == "true" || spinValue == "1" || spinValue == "yes" || spinValue == "on");
        if (!TryParseFloat(values[11], obj.spinAxis.x, logPrefix, "spinAxis.x")
            || !TryParseFloat(values[12], obj.spinAxis.y, logPrefix, "spinAxis.y")
            || !TryParseFloat(values[13], obj.spinAxis.z, logPrefix, "spinAxis.z")
            || !TryParseFloat(values[14], obj.spinSpeed, logPrefix, "spinSpeed"))
        {
            continue;
        }
        obj.currentRotation = 0.0f;
        if (values.size() > 15)
        {
            obj.colliderPresetId = NormalizeColliderKey(values[15]);
        }

        if (kEnableStageModelVerboseLogging)
        {
            DebugTrace(logPrefix + "Stage model loading: " + obj.modelPath + "\n");
        }

        obj.model = request.modelCache.Get(obj.modelPath);
        if (!obj.model)
        {
            DebugTrace(logPrefix + "モデルのロードに失敗: " + obj.modelPath + "\n");
            ++result.modelsFailed;
        }
        else
        {
            ++result.modelsLoaded;
            if (kEnableStageModelVerboseLogging)
            {
                DebugTrace(logPrefix + "Stage model loaded: " + obj.modelPath + "\n");
            }
        }

        if (request.enablePhysics)
        {
            const std::string modelKey = BuildModelKey(obj.modelPath);
            const auto preset = ResolveStageColliderPreset(stageColliderConfig, obj.colliderPresetId, obj.modelPath);
            SceneGameConfig::StageColliderPreset resolvedPreset{};
            std::string registryId = modelKey;
            if (preset)
            {
                resolvedPreset = *preset;
                obj.colliderPresetId = preset->presetId;
                if (!preset->registryId.empty())
                {
                    registryId = preset->registryId;
                }
            }

            bool usedModelFallback = false;
            if (preset)
            {
                if (!Physics::ColliderRegistry::Instance().Find(registryId))
                {
                    std::ostringstream oss;
                    oss << logPrefix << "[StageColliders] registryId='" << registryId
                        << "' missing for model '" << obj.modelPath
                        << "' (preset='" << resolvedPreset.presetId << "').\n";
                    DebugTrace(oss.str());
                }
            }
            else
            {
                usedModelFallback = true;
                std::ostringstream fallbackLog;
                fallbackLog << logPrefix << "[StageColliders] fallback registryId='" << modelKey
                    << "' from modelKey\n";
                DebugTrace(fallbackLog.str());
            }

            const std::string colliderKey = usedModelFallback ? modelKey : registryId;
            Physics::ColliderBuilder colliderBuilder(
                [&](const std::string& message)
                {
                    DebugTrace(message);
                },
                logPrefix.c_str());

            Physics::ColliderBuilder::RegistryBuildRequest colliderRequest{};
            colliderRequest.key = colliderKey;
            colliderRequest.transform = obj.transform;
            colliderRequest.bodyType = resolvedPreset.bodyType;
            colliderRequest.fixedRotation = resolvedPreset.fixedRotation;
            colliderRequest.allowRotation = resolvedPreset.allowRotation;
            colliderRequest.angularDamping = resolvedPreset.allowRotation ? 0.2f : 0.0f;
            colliderRequest.linearDamping = 0.0f;
            colliderRequest.offset = float3{ resolvedPreset.offset.x, resolvedPreset.offset.y, 0.0f };
            colliderRequest.scaleOffset = false;
            colliderRequest.allowFallback = false;

            auto colliderBuild = colliderBuilder.BuildFromRegistry(colliderRequest);
            const bool colliderReady = colliderBuild.collider && !colliderBuild.collider->Empty()
                && colliderBuild.shapeConfig && colliderBuild.shapeConfig->type != Physics::Box2DShapeConfig::Type::None;

            if (colliderReady)
            {
                const std::string presetId = !resolvedPreset.presetId.empty() ? resolvedPreset.presetId : colliderKey;
                auto entity = std::make_shared<Entities::PhysicalEntity>(
                    obj.transform,
                    colliderBuild.shapeConfig,
                    resolvedPreset.layer,
                    obj.model,
                    nullptr,
                    Core::UpdatePhase::PrePhysics,
                    0,
                    presetId,
                    false);

                entity->SetBoxColliderComponent(colliderBuild.collider);
                entity->SetAllowMeshPhysics(false);
                result.physicsEntities.push_back(entity);

                std::ostringstream oss;
                oss << logPrefix << "[StageColliders] Registered registryId='" << colliderKey
                    << "' for model '" << obj.modelPath << "' (layer=" << static_cast<int>(resolvedPreset.layer)
                    << ")\n";
                DebugTrace(oss.str());
            }
        }

        result.stageObjects.push_back(std::move(obj));
    }

    DebugTrace(logPrefix + "ステージオブジェクトを" + std::to_string(result.stageObjects.size()) + "個ロードしました\n");
    DebugTrace(logPrefix + "Loaded stage models: success=" + std::to_string(result.modelsLoaded)
        + ", failed=" + std::to_string(result.modelsFailed) + "\n");

    result.success = true;
    return result;
}
