#ifndef SCENE_GAME_CONFIG_LOADER_H_
#define SCENE_GAME_CONFIG_LOADER_H_

#include "SceneGameConfigTypes.h"

#include <string>

namespace SceneGameConfig
{
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
        bool continuousCollisionForPlayer = false;
    };

    PlayerColliderTuning LoadPlayerColliderTuning(const std::string& path);
    PlayerSpawnTransform LoadPlayerTransformConfig();
    PhysicsTuningSettings LoadPhysicsTuningSettings();
    bool LoadModelTextureLoadingSetting();
    RenderPostProcessSettings LoadRenderPostProcessSettings();
    LightingSettings LoadLightingSettings();
    BlobShadowSettings LoadBlobShadowSettings();
    MaterialTuningSettings LoadMaterialTuningSettings();
    CameraRigSettings LoadCameraRigSettings();
    GameCameraConfig LoadGameCameraConfig();
}

#endif
