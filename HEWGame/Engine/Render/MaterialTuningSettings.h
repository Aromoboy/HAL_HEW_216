#pragma once

#include <DirectXMath.h>
#include <map>
#include <string>

struct MaterialTuningProfile
{
    float lightResponse = 1.0f;
    float sssIntensity = 0.0f;
    DirectX::XMFLOAT3 sssColor{ 1.0f, 1.0f, 1.0f };
};

struct MaterialTuningSettings
{
    std::map<std::string, MaterialTuningProfile> profiles;
    std::map<std::string, std::string> overrides;
};
