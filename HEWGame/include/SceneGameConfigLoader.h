#ifndef SCENE_GAME_CONFIG_LOADER_H_
#define SCENE_GAME_CONFIG_LOADER_H_

#include "SceneGame.h"
#include "CollisionLayer.h"

#include <box2d/box2d.h>

#include <string>
#include <unordered_map>

namespace SceneGameConfig
{
    struct StageColliderPreset
    {
        std::string presetId;
        std::string registryId;
        Collision::CollisionLayer layer{ Collision::CollisionLayer::Floor };
        b2BodyType bodyType{ b2_staticBody };
        bool fixedRotation{ true };
        bool allowRotation{ false };
        b2Vec2 offset{ 0.0f, 0.0f };
    };

    struct StageColliderConfig
    {
        std::unordered_map<std::string, StageColliderPreset> presetsByModelKey;
        std::unordered_map<std::string, StageColliderPreset> presetsById;
    };

    struct PhysicsTuningSettings
    {
        float gravity = 11.5f;
        int subStepCount = 8;
        float linearDamping = 0.97f;
        float angularDamping = 0.97f;
        float defaultFriction = 0.52f;
        float defaultRestitution = 0.06f;
        float restitutionThreshold = 5.0f;
        float hitEventThreshold = 0.0f;
        float maxStepDt = 1.0f / 60.0f;
        int maxAccumulatedSteps = 8;
        bool continuousCollisionForSweets = true;
        bool continuousCollisionForSmallDynamics = false;
        float smallDynamicContinuousThreshold = 0.6f;
        bool continuousCollisionForPlayerProjectiles = true;
    };

    StageColliderConfig LoadStageColliderConfig(const std::string& path);
    SceneGame::PlayerColliderTuning LoadPlayerColliderTuning(const std::string& path);
    SceneGame::PlayerSpawnTransform LoadPlayerTransformConfig();
    PhysicsTuningSettings LoadPhysicsTuningSettings();
    bool LoadModelTextureLoadingSetting();
}

#endif
