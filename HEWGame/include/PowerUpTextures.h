#pragma once

#include <string>
#include "TextureCache.h"

namespace PowerUpTextures
{
constexpr const char* kStarPath = "Assets/Texture/Star.png";
constexpr const char* kCostPath = "Assets/Texture/upgrade_UI_pointUP_icon.png";
constexpr const char* kSpeedUpPath = "Assets/Texture/upgrade_UI_MoveSpeed_icon.png";
constexpr const char* kCakeSizePath = "Assets/Texture/upgrade_UI_cakesizeUpai_icon.png";
constexpr const char* kWhipPath = "Assets/Texture/upgrade_UI_cream_plus_icon.png";
constexpr const char* kFramePath = "Assets/Texture/UI_whiteblock.png";
constexpr const char* kBackFramePath = "Assets/Texture/UI_Cakeblock.png";

inline std::string BuildKey(const char* path)
{
    return TextureCache::MakeFileKey(path);
}
}
