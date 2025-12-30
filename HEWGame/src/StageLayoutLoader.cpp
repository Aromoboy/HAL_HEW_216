#include "StageLayoutLoader.h"

#include "BoxColliderComponent.h"
#include "ColliderRegistry.h"
#include "ModelCache.h"
#include "SceneGameConfigLoader.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>

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
            key = path.filename().string();
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

    Physics::Box2DShapeConfig::Type InferShapeType(const std::vector<Physics::ColliderShapeDescriptor>& shapes)
    {
        if (shapes.empty())
        {
            return Physics::Box2DShapeConfig::Type::None;
        }

        switch (shapes.front().type)
        {
        case Physics::ColliderShapeType::Box:
            return Physics::Box2DShapeConfig::Type::Box;
        case Physics::ColliderShapeType::Polygon:
            return Physics::Box2DShapeConfig::Type::Polygon;
        case Physics::ColliderShapeType::Circle:
            return Physics::Box2DShapeConfig::Type::Circle;
        default:
            return Physics::Box2DShapeConfig::Type::None;
        }
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

        StageObject obj;
        obj.modelPath = values[0];

        obj.transform.position.x = std::stof(values[1]);
        obj.transform.position.y = std::stof(values[2]);
        obj.transform.position.z = std::stof(values[3]);

        obj.transform.rotation.x = std::stof(values[4]);
        obj.transform.rotation.y = std::stof(values[5]);
        obj.transform.rotation.z = std::stof(values[6]);

        obj.transform.scale.x = std::stof(values[7]);
        obj.transform.scale.y = std::stof(values[8]);
        obj.transform.scale.z = std::stof(values[9]);

        std::string spinValue = values[10];
        std::transform(spinValue.begin(), spinValue.end(), spinValue.begin(), ::tolower);

        obj.spinEnabled = (spinValue == "true" || spinValue == "1" || spinValue == "yes" || spinValue == "on");
        obj.spinAxis.x = std::stof(values[11]);
        obj.spinAxis.y = std::stof(values[12]);
        obj.spinAxis.z = std::stof(values[13]);
        obj.spinSpeed = std::stof(values[14]);
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
            continue;
        }
        ++result.modelsLoaded;
        if (kEnableStageModelVerboseLogging)
        {
            DebugTrace(logPrefix + "Stage model loaded: " + obj.modelPath + "\n");
        }

        if (request.enablePhysics)
        {
            const auto preset = ResolveStageColliderPreset(stageColliderConfig, obj.colliderPresetId, obj.modelPath);
            if (preset)
            {
                obj.colliderPresetId = preset->presetId;

                auto registryDefinition = Physics::ColliderRegistry::Instance().Find(preset->registryId);
                if (!registryDefinition)
                {
                    std::ostringstream oss;
                    oss << logPrefix << "[StageColliders] registryId='" << preset->registryId
                        << "' missing for model '" << obj.modelPath << "' (preset='" << preset->presetId << "').\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    Physics::BoxColliderComponent colliderComponent{};
                    colliderComponent.colliderId = preset->registryId;
                    colliderComponent.shapeDef = registryDefinition->shapeDef;
                    colliderComponent.shapes = registryDefinition->shapes;
                    colliderComponent.colliderOffset = preset->offset;

                    Physics::Box2DShapeConfig shapeConfig{};
                    shapeConfig.type = InferShapeType(colliderComponent.shapes);
                    shapeConfig.bodyType = preset->bodyType;
                    shapeConfig.fixedRotation = preset->fixedRotation;
                    shapeConfig.allowRotation = preset->allowRotation;
                    shapeConfig.angularDamping = preset->allowRotation ? 0.2f : 0.0f;
                    shapeConfig.density = std::max(0.0f, registryDefinition->shapeDef.density);
                    shapeConfig.friction = std::max(0.0f, registryDefinition->shapeDef.material.friction);
                    shapeConfig.restitution = std::clamp(registryDefinition->shapeDef.material.restitution, 0.0f, 1.0f);

                    auto entity = std::make_shared<Entities::PhysicalEntity>(
                        obj.transform,
                        shapeConfig,
                        preset->layer,
                        obj.model,
                        nullptr,
                        Core::UpdatePhase::PrePhysics,
                        0,
                        preset->presetId,
                        false);

                    entity->SetBoxColliderComponent(colliderComponent);
                    entity->SetAllowMeshPhysics(false);
                    result.physicsEntities.push_back(entity);

                    std::ostringstream oss;
                    oss << logPrefix << "[StageColliders] Registered preset='" << preset->presetId
                        << "' for model '" << obj.modelPath << "' (layer=" << static_cast<int>(preset->layer)
                        << ")\n";
                    DebugTrace(oss.str());
                }
            }
            else
            {
                std::ostringstream oss;
                oss << logPrefix << "[StageColliders] No preset mapped for model '" << obj.modelPath
                    << "'; collider skipped.\n";
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
