#ifndef SCENE_GAME_CONFIG_TYPES_H_
#define SCENE_GAME_CONFIG_TYPES_H_

#include "Transform.h"

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
        bool enableFog = true;
        bool enableVignette = true;
        bool enableBloom = true;
    };
}

#endif
