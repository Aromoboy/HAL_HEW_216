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
        bool continuousCollisionForMeteors = true;
        bool continuousCollisionForSmallDynamics = false;
        float smallDynamicContinuousThreshold = 0.6f;
        bool continuousCollisionForPlayerProjectiles = true;
        bool continuousCollisionForPlayer = false;
        float creamGlueLinearHertz = 0.0f;
        float creamGlueLinearDampingRatio = 0.7f;
        float creamGlueAngularHertz = 0.0f;
        float creamGlueAngularDampingRatio = 0.7f;
        bool creamGlueCollideConnected = false;
        float creamGluePushOutEpsilon = 0.03f;
        float creamGlueMaxPushOut = 0.25f;
        float creamGlueMaxSpeed = 0.0f;
        float creamGlueBreakForce = 0.0f;
        float creamGlueBreakImpulse = 0.0f;
    };

    PlayerColliderTuning LoadPlayerColliderTuning(const std::string& path);
    PlayerSpawnTransform LoadPlayerTransformConfig();
    MissileSettings LoadMissileSettings();
    PhysicsTuningSettings LoadPhysicsTuningSettings();
    bool LoadModelTextureLoadingSetting();
    ModelImportSettings LoadModelImportSettings();
    RenderPostProcessSettings LoadRenderPostProcessSettings();
    RenderFadeSettings LoadRenderFadeSettings();
    LightingSettings LoadLightingSettings();
    BlobShadowSettings LoadBlobShadowSettings();
    CameraRigSettings LoadCameraRigSettings();
    GameCameraConfig LoadGameCameraConfig();
}

#endif
