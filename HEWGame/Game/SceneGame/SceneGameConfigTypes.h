#ifndef SCENE_GAME_CONFIG_TYPES_H_
#define SCENE_GAME_CONFIG_TYPES_H_

#include "Transform.h"
#include "Systems/Physics/CollisionLayer.h"
#include <DirectXMath.h>
#include <optional>
#include <string>

namespace SceneGameConfig
{
    struct PlayerColliderTuning
    {
        float thicknessPadding = 1.0f;
        float3 padding{ 0.0f, 0.0f, 0.0f };
        float3 offset{ 0.0f, 0.0f, 0.0f };
        float3 fallbackPadding{ 0.0f, 0.0f, 0.0f };
        float3 fallbackOffset{ 0.0f, 0.0f, 0.0f };
        float3 fallbackSize{ 1.0f, 1.0f, 1.0f };
    };

    struct PlayerSpawnTransform
    {
        Transform transform{ float3{ 0.0f, 0.0f, 0.0f }, float3{ 0.0f, 0.0f, 0.0f }, float3{ 1.0f, 1.0f, 1.0f } };
        float3 missileSpawnOffset{ 1.5f, 0.0f, 0.0f };
        bool loadedFromFile = false;
        bool rotationFromDegrees = false;
        float playerYawOffsetRadians = 0.0f;
        float missileYawOffsetRadians = 0.0f;
    };

    struct MissileSettings
    {
        float3 scale{ 1.0f, 1.0f, 1.0f };
        float rotationOffset = 0.0f;
        float halfWidth = 0.2f;
        float halfHeight = 0.1f;
        float density = 1.0f;
        float friction = 0.1f;
        float restitution = 0.0f;
        bool fixedRotation = true;
        float linearDamping = 0.0f;
        float angularDamping = 0.0f;
        float gravityScale = 0.0f;
        std::optional<Collision::CollisionLayer> collisionLayer = std::nullopt;
        bool loadedFromFile = false;
    };

    struct RenderPostProcessSettings
    {
        float gamma = 2.2f;
        float saturation = 0.85f;
        float contrast = 1.1f;
        float3 fogColor{ 1.0f, 1.0f, 1.0f };
        float fogStart = 50.0f;
        float fogRange = 50.0f;
        float vignetteStrength = 1.2f;
        float bloomThreshold = 0.8f;
        float bloomIntensity = 0.15f;
        float grainAmount = 0.0f;
        float fxaaEdgeThreshold = 0.166f;
        float fxaaEdgeThresholdMin = 0.0833f;
        float fxaaSubpixelQuality = 0.75f;
        bool enableFog = true;
        bool enableVignette = true;
        bool enableBloom = true;
        bool grainEnable = false;
        bool enableFxaa = false;
    };

    struct ModelImportSettings
    {
        int importTimeoutMs = 2000;
    };

    enum class RenderFadeEasing
    {
        Linear,
        SmoothStep,
        EaseIn,
        EaseOut,
        EaseInOut,
    };

    struct RenderFadeSettings
    {
        bool enable = true;
        float fadeInSeconds = 0.5f;
        float fadeOutSeconds = 0.5f;
        float holdSeconds = 0.0f;
        float3 color{ 0.0f, 0.0f, 0.0f };
        RenderFadeEasing easing = RenderFadeEasing::Linear;
    };

    struct LightingSettings
    {
        int mode = 1;
        DirectX::XMFLOAT4 keyDiffuse{ 1.0f, 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 keyDir{ 0.0f, 0.0f, 1.0f };
        float keyWrap = 0.0f;
        bool keyEnable = true;
        DirectX::XMFLOAT4 fillDiffuse{ 0.0f, 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 fillDir{ 0.0f, 0.0f, 1.0f };
        float fillWrap = 0.0f;
        bool fillEnable = true;
        DirectX::XMFLOAT4 hemiSky{ 0.0f, 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 hemiGround{ 0.0f, 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 rimColor{ 0.0f, 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT4 rimParams{ 0.0f, 0.0f, 0.0f, 0.0f };
        bool rimEnable = false;
    };

    struct BlobShadowSettings
    {
        bool enable = true;
        bool debugNoDepthTest = false;
        bool forceReceiverZ = false;
        float opacity = 0.35f;
        float softness = 0.4f;
        float maxHeight = 6.0f;
        float minScale = 0.35f;
        float receiverBias = 0.01f;
        float groundY = 0.0f;
        std::optional<float> groundZ = std::nullopt;
    };

    struct CameraClampSettings
    {
        std::optional<float> minY = std::nullopt;
        std::optional<float> maxY = std::nullopt;
    };

    enum class CameraRigMode
    {
        Fixed,
        Follow,
        Cinematic,
    };

    struct CameraRigSettings
    {
        CameraRigMode mode = CameraRigMode::Follow;
        bool fixedPosition = false;
        bool lockTranslation = false;
        bool lockYawPitch = false;
        float followSpeed = 5.0f;
        float3 offset{ 0.0f, 5.5f, -25.0f };
        float fovDegrees = 45.0f;
        float yawDegrees = 0.0f;
        float pitchDegrees = -10.0f;
        bool wrapY = false;
        CameraClampSettings clamp{};
    };

    struct GameCameraConfig
    {
        float comfortZoneTopPercentY = 0.1f;
        float comfortZoneBottomPercentY = 0.1f;
        float followSpeedY = 5.0f;
        float offsetY = 0.0f;
    };

}

#endif
