#include "StageLayoutLoader.h"

#include "ColliderRegistry.h"
#include "CollisionLayer.h"
#include "ModelCache.h"
#include "SceneGameConfigLoader.h"
#include "Systems/Physics/ColliderBuilder.h"
#include "Defines.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <box2d/box2d.h>

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
    constexpr const char* kFallbackStageMaterialKey = "default";

    bool IsKnownStageMaterialKey(const std::string& key, const MaterialTuningSettings& settings)
    {
        return settings.overrides.contains(key) || settings.profiles.contains(key);
    }

    std::string NormalizeKey(const std::string& key)
    {
        std::string normalized;
        normalized.reserve(key.size());
        for (unsigned char ch : key)
        {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
        }
        return normalized;
    }

    std::string Trim(const std::string& value)
    {
        auto begin = value.begin();
        auto end = value.end();
        while (begin != end && std::isspace(static_cast<unsigned char>(*begin)))
        {
            ++begin;
        }
        while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1))))
        {
            --end;
        }
        return std::string(begin, end);
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

        return NormalizeKey(key);
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

    bool TryParseAngle(const std::string& token, float& value, const std::string& logPrefix, const char* fieldName)
    {
        if (token.empty())
        {
            DebugTrace(logPrefix + "StageLayout.csv empty token for " + fieldName + "\n");
            return false;
        }

        try
        {
            value = std::stof(token);
            value *= DEG_TO_RAD;
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
    const SceneGameConfig::MaterialTuningSettings materialTuningSettings = SceneGameConfig::LoadMaterialTuningSettings();

    constexpr Collision::CollisionLayer kStageColliderLayer = Collision::CollisionLayer::Stage;
    constexpr b2BodyType kStageBodyType = b2_staticBody;
    //Osara用
    constexpr b2BodyType kOsaraBodyType = b2_kinematicBody;
    constexpr bool kStageFixedRotation = true;
    constexpr bool kStageAllowRotation = false;

    std::ifstream file(request.csvPath);
    if (!file.is_open())
    {
        DebugTrace(logPrefix + "StageLayout.csvを開けませんでした\n");
        return result;
    }

    DebugTrace(logPrefix + "CSVファイルを開きました: " + request.csvPath + "\n");

    if (request.enablePhysics)
    {
        const bool registryLoaded = Physics::ColliderRegistry::Instance().LoadFromFile(request.gameplayConfigPath);
        if (!registryLoaded)
        {
            DebugTrace(logPrefix + "Stage collider registry missing; falling back to cached definitions if any.\n");
        }
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

        std::string lower = obj.modelPath;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        obj.IsOsara = (lower.find("osara") != std::string::npos);

        if (!TryParseFloat(values[1], obj.transform.position.x, logPrefix, "position.x")
            || !TryParseFloat(values[2], obj.transform.position.y, logPrefix, "position.y")
            || !TryParseFloat(values[3], obj.transform.position.z, logPrefix, "position.z")
            || !TryParseAngle(values[4], obj.transform.rotation.x, logPrefix, "rotation.x")
            || !TryParseAngle(values[5], obj.transform.rotation.y, logPrefix, "rotation.y")
            || !TryParseAngle(values[6], obj.transform.rotation.z, logPrefix, "rotation.z")
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
            obj.materialKey = Trim(values[15]);
        }
        if (values.size() > 16)
        {
            obj.colliderPresetId = NormalizeKey(values[16]);
        }
        if (!obj.materialKey.empty())
        {
            const std::string normalizedMaterialKey = NormalizeKey(obj.materialKey);
            if (IsKnownStageMaterialKey(normalizedMaterialKey, materialTuningSettings))
            {
                obj.materialKey = normalizedMaterialKey;
            }
            else
            {
                obj.materialKey.clear();
            }
        }
        if (obj.materialKey.empty())
        {
            obj.materialKey = kFallbackStageMaterialKey;
        }

        const std::string modelKey = BuildModelKey(obj.modelPath);
        const std::string colliderKey = obj.colliderPresetId.empty() ? modelKey : obj.colliderPresetId;
        if (obj.IsOsara)
        {
            const auto registryDefinition = Physics::ColliderRegistry::Instance().Find(colliderKey);
            if (registryDefinition)
            {
                obj.OsaraSwayingpower = registryDefinition->OsaraSwayingpower;
            }
            else
            {
                std::ostringstream oss;
                oss << logPrefix << "[StageColliders] registryId='" << colliderKey
                    << "' missing for Osara; using default OsaraSwayingpower ("
                    << obj.OsaraSwayingpower.x << ", " << obj.OsaraSwayingpower.y << ").\n";
                DebugTrace(oss.str());
            }
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
            obj.model->SetMaterialKey(obj.materialKey);
            ++result.modelsLoaded;
            if (kEnableStageModelVerboseLogging)
            {
                DebugTrace(logPrefix + "Stage model loaded: " + obj.modelPath + "\n");
            }
        }

        if (request.enablePhysics)
        {
            if (!Physics::ColliderRegistry::Instance().Find(colliderKey))
            {
                std::ostringstream oss;
                oss << logPrefix << "[StageColliders] registryId='" << colliderKey
                    << "' missing for model '" << obj.modelPath << "'.\n";
                DebugTrace(oss.str());
            }

            Physics::ColliderBuilder colliderBuilder(
                [&](const std::string& message)
                {
                    DebugTrace(message);
                },
                logPrefix.c_str());

            Physics::ColliderBuilder::RegistryBuildRequest colliderRequest{};
            colliderRequest.key = colliderKey;
            colliderRequest.transform = obj.transform;
            //colliderRequest.bodyType = kStageBodyType;
            colliderRequest.bodyType = obj.IsOsara ? kOsaraBodyType : kStageBodyType;
            colliderRequest.fixedRotation = kStageFixedRotation;
            colliderRequest.allowRotation = kStageAllowRotation;
            colliderRequest.angularDamping = kStageAllowRotation ? 0.2f : 0.0f;
            colliderRequest.linearDamping = 0.0f;
            colliderRequest.offset = float3{ 0.0f, 0.0f, 0.0f };
            colliderRequest.scaleOffset = false;
            colliderRequest.allowFallback = false;

            auto colliderBuild = colliderBuilder.BuildFromRegistry(colliderRequest);
            const bool colliderReady = colliderBuild.collider && !colliderBuild.collider->Empty()
                && colliderBuild.shapeConfig && colliderBuild.shapeConfig->type != Physics::Box2DShapeConfig::Type::None;

            if (colliderReady)
            {
                const std::string presetId = colliderKey;
                auto entity = std::make_shared<Entities::PhysicalEntity>(
                    obj.transform,
                    colliderBuild.shapeConfig,
                    kStageColliderLayer,
                    obj.model,
                    nullptr,
                    Core::UpdatePhase::PrePhysics,
                    0,
                    presetId,
                    false);

                entity->SetBoxColliderComponent(colliderBuild.collider);
                entity->SetAllowMeshPhysics(false);
                //紐づけ
                obj.physicsEntity = entity;
                result.physicsEntities.push_back(entity);

                std::ostringstream oss;
                oss << logPrefix << "[StageColliders] Registered registryId='" << colliderKey
                    << "' for model '" << obj.modelPath << "' (layer=" << static_cast<int>(kStageColliderLayer)
                    << ")\n";
                DebugTrace(oss.str());
            }
        }
        //baseYを保存
        obj.baseY = obj.transform.position.y;
        
        result.stageObjects.push_back(std::move(obj));
    }

    DebugTrace(logPrefix + "ステージオブジェクトを" + std::to_string(result.stageObjects.size()) + "個ロードしました\n");
    DebugTrace(logPrefix + "Loaded stage models: success=" + std::to_string(result.modelsLoaded)
        + ", failed=" + std::to_string(result.modelsFailed) + "\n");

    result.success = true;
    return result;
}
