#ifndef SCENE_GAME_CONFIG_TYPES_H_
#define SCENE_GAME_CONFIG_TYPES_H_

#include "Transform.h"
#include "MaterialTuningSettings.h"

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
        bool loadedFromFile = false;
        bool rotationFromDegrees = false;
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
        bool grainAnimate = false;
        bool enableFxaa = false;
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
        float opacity = 0.35f;
        float softness = 0.4f;
        float maxHeight = 6.0f;
        float minScale = 0.35f;
        float receiverBias = 0.01f;
        float groundY = 0.0f;
        std::optional<float> groundZ = std::nullopt;
    };

    using MaterialTuningProfile = ::MaterialTuningProfile;
    using MaterialTuningSettings = ::MaterialTuningSettings;
}

#endif
