#pragma once

#include <DirectXMath.h>
#include <map>
#include <optional>
#include <string>

struct MaterialTuningProfile
{
    float lightResponse = 1.0f;
    float sssIntensity = 0.0f;
    DirectX::XMFLOAT3 sssColor{ 1.0f, 1.0f, 1.0f };
};

struct MaterialTuningOverride
{
    std::string profileName;
    std::optional<float> lightResponse;
    std::optional<float> sssIntensity;
    std::optional<DirectX::XMFLOAT3> sssColor;
};

struct MaterialTuningSettings
{
    std::map<std::string, MaterialTuningProfile> profiles;
    std::map<std::string, MaterialTuningOverride> overrides;
};
