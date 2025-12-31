// ステージ配置を読み込んでデータ化する
// シーンゲームの初期化で使われる

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ModelDX.h"
#include "Transform.h"
#include "Vector.h"
#include "Entities/PhysicalEntity.h"

namespace Render
{
    class ModelCache;
}

struct StageObject
{
    std::string modelPath;
    Transform transform;
    bool spinEnabled = false;
    float3 spinAxis{ 0.0f, 0.0f, 0.0f };
    float spinSpeed = 0.0f;
    std::shared_ptr<ModelDX> model;
    float currentRotation = 0.0f;
    std::string colliderPresetId;
};

struct StageLayoutLoadRequest
{
    std::string csvPath;
    std::string gameplayConfigPath;
    Render::ModelCache& modelCache;
    bool enablePhysics = false;
    std::string logPrefix;
};

struct StageLayoutLoadResult
{
    bool success = false;
    std::vector<StageObject> stageObjects;
    std::vector<std::shared_ptr<Entities::PhysicalEntity>> physicsEntities;
    size_t modelsLoaded = 0;
    size_t modelsFailed = 0;
};

class StageLayoutLoader
{
public:
    static StageLayoutLoadResult LoadFromCsv(const StageLayoutLoadRequest& request);
};
