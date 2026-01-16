#pragma once

namespace GameplayConfig
{
    inline constexpr const char* kConfigPath = "Assets/Config/gameplay.cfg";
    inline constexpr const char* kSectionPhysicsTuning = "physics.tuning";
    inline constexpr const char* kSectionPlayerTransform = "player.transform";
    inline constexpr const char* kSectionPlayerMovement = "player.movement";
    inline constexpr const char* kSectionCameraRig = "camera.rig";
    inline constexpr const char* kPlayerTransformKeyYawOffsetDegrees = "yawOffsetDegrees";
    inline constexpr const char* kPlayerTransformKeyMissileYawOffsetDegrees = "missileYawOffsetDegrees";
    inline constexpr const char* kPlayerMovementKeyInputSmoothing = "inputSmoothing";
    inline constexpr const char* kPlayerMovementKeyAccel = "accel";
    inline constexpr const char* kPlayerMovementKeyBrake = "brake";
    inline constexpr const char* kPlayerMovementKeyTurnRate = "turnRate";
    inline constexpr const char* kPlayerMovementKeyRollResponse = "rollResponse";
    inline constexpr const char* kCameraRigKeyMode = "mode";
    inline constexpr const char* kCameraRigKeyMinY = "minY";
    inline constexpr const char* kCameraRigKeyMaxY = "maxY";
    inline constexpr const char* kCameraRigKeyWrapY = "wrapY";
    inline constexpr const char* kCameraRigKeyFollowSpeed = "followSpeed";
    inline constexpr const char* kCameraRigKeyComfortZoneTopPercentY = "comfortZoneTopPercentY";
    inline constexpr const char* kCameraRigKeyComfortZoneBottomPercentY = "comfortZoneBottomPercentY";
    inline constexpr const char* kCameraRigKeyFollowSpeedY = "followSpeedY";
    inline constexpr const char* kCameraRigKeyOffsetY = "offsetY";
    inline constexpr const char* kCameraRigKeyOffset = "offset";
    inline constexpr const char* kCameraRigKeyFovDegrees = "fovDegrees";
    inline constexpr const char* kCameraRigKeyYawDegrees = "yawDegrees";
    inline constexpr const char* kCameraRigKeyPitchDegrees = "pitchDegrees";
    inline constexpr const char* kSectionMissileSettings = "missile.settings";
    inline constexpr const char* kMissileKeyScale = "scale";
    inline constexpr const char* kMissileKeyRotationOffset = "rotationOffset";
    inline constexpr const char* kMissileKeyHalfWidth = "halfWidth";
    inline constexpr const char* kMissileKeyHalfHeight = "halfHeight";
    inline constexpr const char* kMissileKeyDensity = "density";
    inline constexpr const char* kMissileKeyFriction = "friction";
    inline constexpr const char* kMissileKeyRestitution = "restitution";
    inline constexpr const char* kMissileKeyFixedRotation = "fixedRotation";
    inline constexpr const char* kMissileKeyLinearDamping = "linearDamping";
    inline constexpr const char* kMissileKeyAngularDamping = "angularDamping";
    inline constexpr const char* kMissileKeyGravityScale = "gravityScale";
    inline constexpr const char* kMissileKeyCollisionLayer = "collisionLayer";
    inline constexpr const char* kSectionStageChecklist = "stage.checklist";
    inline constexpr const char* kSectionSpawnPhases = "spawn.phases";
    inline constexpr const char* kSectionMeteorsManifest = "meteors.manifest";
    inline constexpr const char* kSectionColliderRegistry = "colliders.registry";
    inline constexpr const char* kSectionPlayerColliders = "colliders.player";
    inline constexpr const char* kSectionPlayerColliderTuning = "colliders.player.tuning";
    inline constexpr const char* kSectionMeteorsFactory = "colliders.meteors.factory";
}
