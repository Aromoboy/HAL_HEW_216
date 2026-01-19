#pragma once

namespace GameplayConfig
{
    inline constexpr const char* kConfigPath = "Assets/Config/gameplay.cfg";
    inline constexpr const char* kSectionPhysicsTuning = "physics.tuning";
    inline constexpr const char* kSectionPlayerTransform = "player.transform";
    inline constexpr const char* kSectionCameraRig = "camera.rig";
    inline constexpr const char* kCameraRigKeyMode = "mode";
    inline constexpr const char* kCameraRigKeyMinY = "minY";
    inline constexpr const char* kCameraRigKeyMaxY = "maxY";
    inline constexpr const char* kCameraRigKeyFollowSpeed = "followSpeed";
    inline constexpr const char* kCameraRigKeyComfortZoneTopPercentY = "comfortZoneTopPercentY";
    inline constexpr const char* kCameraRigKeyComfortZoneBottomPercentY = "comfortZoneBottomPercentY";
    inline constexpr const char* kCameraRigKeyFollowSpeedY = "followSpeedY";
    inline constexpr const char* kCameraRigKeyOffsetY = "offsetY";
    inline constexpr const char* kCameraRigKeyOffset = "offset";
    inline constexpr const char* kCameraRigKeyFovDegrees = "fovDegrees";
    inline constexpr const char* kCameraRigKeyYawDegrees = "yawDegrees";
    inline constexpr const char* kCameraRigKeyPitchDegrees = "pitchDegrees";
    inline constexpr const char* kSectionStageChecklist = "stage.checklist";
    inline constexpr const char* kSectionSpawnPhases = "spawn.phases";
    inline constexpr const char* kSectionPowerUps = "powerups";
    inline constexpr const char* kSectionSweetsLifecycle = "sweets.lifecycle";
    inline constexpr const char* kSectionSweetsManifest = "sweets.manifest";
    inline constexpr const char* kSectionColliderRegistry = "colliders.registry";
    inline constexpr const char* kSectionPlayerColliders = "colliders.player";
    inline constexpr const char* kSectionPlayerColliderTuning = "colliders.player.tuning";
    inline constexpr const char* kSectionSweetsFactory = "colliders.sweets.factory";
}
