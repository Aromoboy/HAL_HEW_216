#include "Stage/StageData.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <cstring>
#include <iterator>
#include <sstream>
#include <stdexcept>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
    void DebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    void DebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

    constexpr const char* kLogPrefix = "[StageData] ";
    constexpr float kDefaultPlaneZ = -5.0f;
    constexpr float kDefaultPlatformCenterY = -1.0f;
    constexpr float kDefaultPlatformWidthFraction = 0.20f;
    constexpr float kDefaultPlatformHeightFraction = 0.05f;
    constexpr float kDefaultPlatformDepth = 4.0f;
    const float3 kDefaultPlatformScale{ 12.0f, 2.0f, 4.0f };
    constexpr float kDefaultPlayerMoveSpeed = 15.0f;
    constexpr float kDefaultGuidelineHeight = 20.0f;
    constexpr float kDefaultHeightBonusThreshold = kDefaultGuidelineHeight;
    constexpr float kDefaultHeightBonusSeconds = 5.0f;
    constexpr const char* kDefaultBackgroundTexturePath = "Assets/Texture/Title.jpg";

    const float3 kDefaultSpawnPoints[] =
    {
        { -2.5f, 0.0f, kDefaultPlaneZ },
        { 2.5f, 0.0f, kDefaultPlaneZ },
    };

    const float kDefaultThresholds[] = { 5.0f, 15.0f, 30.0f };
    const float kDefaultRewardMultipliers[] = { 1.0f };

    std::string Trim(const std::string& value)
    {
        auto begin = value.begin();
        auto end = value.end();
        while (begin != end && std::isspace(static_cast<unsigned char>(*begin)))
        {
            ++begin;
        }
        while (end != begin)
        {
            auto last = end;
            --last;
            if (!std::isspace(static_cast<unsigned char>(*last)))
            {
                break;
            }
            end = last;
        }
        return std::string(begin, end);
    }

    std::vector<float> ParseFloatList(const std::string& source, bool* outError)
    {
        std::vector<float> values;
        std::string token;
        std::istringstream stream(source);
        while (std::getline(stream, token, ','))
        {
            token = Trim(token);
            if (token.empty())
            {
                continue;
            }
            try
            {
                values.push_back(std::stof(token));
            }
            catch (const std::exception& ex)
            {
                if (outError)
                {
                    *outError = true;
                }
                std::ostringstream oss;
                oss << kLogPrefix << "Failed to parse float token '" << token
                    << "' in list '" << source << "': " << ex.what() << "\n";
                DebugTrace(oss.str());
                return {};
            }
        }
        if (values.empty() && outError)
        {
            *outError = true;
        }
        return values;
    }

    bool TryParseSpawnPoint(const std::string& rawValue, size_t lineNumber, std::vector<float3>& outPoints)
    {
        std::string normalized = rawValue;
        std::replace(normalized.begin(), normalized.end(), ';', ',');

        bool error = false;
        auto parsed = ParseFloatList(normalized, &error);
        if (error || parsed.size() < 3)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawn point on line " << lineNumber
                << " could not be parsed. Raw='" << rawValue << "'\n";
            DebugTrace(oss.str());
            return false;
        }

        if (parsed.size() % 3 != 0)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawn point on line " << lineNumber
                << " expected groups of three floats but found " << parsed.size()
                << " values. Raw='" << rawValue << "'\n";
            DebugTrace(oss.str());
            return false;
        }

        for (size_t i = 0; i + 2 < parsed.size(); i += 3)
        {
            outPoints.push_back(float3{ parsed[i], parsed[i + 1], parsed[i + 2] });
        }

        return true;
    }

    bool TryParsePlatformKey(const std::string& key, const std::string& field, size_t& outIndex)
    {
        constexpr const char* kPlatformPrefix = "platform";
        if (key.rfind(kPlatformPrefix, 0) != 0)
        {
            return false;
        }

        size_t cursor = std::strlen(kPlatformPrefix);
        size_t parsedIndex = 0;
        bool hasIndex = false;
        while (cursor < key.size() && std::isdigit(static_cast<unsigned char>(key[cursor])))
        {
            parsedIndex = (parsedIndex * 10) + static_cast<size_t>(key[cursor] - '0');
            hasIndex = true;
            ++cursor;
        }

        if (cursor >= key.size() || key[cursor] != '.')
        {
            return false;
        }

        ++cursor;
        const std::string remainder = key.substr(cursor);
        if (remainder != field)
        {
            return false;
        }

        outIndex = hasIndex ? parsedIndex : 0;
        return true;
    }

    struct PlatformOverride
    {
        Stage::PlatformDescriptor descriptor{};
        bool centerProvided = false;
        bool scaleProvided = false;
        bool depthProvided = false;
        bool widthProvided = false;
        bool heightProvided = false;
    };
}

namespace Stage
{
    StageData::StageData()
        : m_planeZ(kDefaultPlaneZ)
        , m_loadedFromFile(false)
        , m_playerMoveSpeed(kDefaultPlayerMoveSpeed)
        , m_guidelineHeight(kDefaultGuidelineHeight)
        , m_backgroundTexturePath(kDefaultBackgroundTexturePath)
        , m_heightBonusThreshold(kDefaultHeightBonusThreshold)
        , m_heightBonusSeconds(kDefaultHeightBonusSeconds)
    {
        ApplyDefaults();
    }

    bool StageData::LoadFromFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Config not found at '" << path << "'. Using defaults.\n";
            DebugTrace(oss.str());
            ApplyDefaults();
            m_sourcePath.clear();
            m_loadedFromFile = false;
            return false;
        }

        std::vector<float> thresholds;
        std::vector<float3> spawnPoints;
        std::vector<float> rewardMultipliers;
        std::vector<PlatformOverride> platformOverrides;
        float planeZ = kDefaultPlaneZ;
        bool planeZSet = false;
        float platformCenterY = kDefaultPlatformCenterY;
        bool platformCenterSet = false;
        float platformWidthFraction = kDefaultPlatformWidthFraction;
        bool platformWidthFractionSet = false;
        float platformHeightFraction = kDefaultPlatformHeightFraction;
        bool platformHeightFractionSet = false;
        float platformDepth = kDefaultPlatformDepth;
        bool platformDepthSet = false;
        float3 platformScale = kDefaultPlatformScale;
        bool platformScaleSet = false;
        float playerMoveSpeed = kDefaultPlayerMoveSpeed;
        bool playerMoveSpeedSet = false;
        float guidelineHeight = kDefaultGuidelineHeight;
        bool guidelineHeightSet = false;
        float heightBonusThreshold = kDefaultHeightBonusThreshold;
        bool heightBonusThresholdSet = false;
        float heightBonusSeconds = kDefaultHeightBonusSeconds;
        bool heightBonusSecondsSet = false;
        std::string backgroundTexturePath = kDefaultBackgroundTexturePath;
        bool backgroundTextureSet = false;
        bool parseError = false;

        std::string line;
        size_t lineNumber = 0;
        while (std::getline(file, line))
        {
            ++lineNumber;
            const auto commentPos = line.find_first_of("#;");
            if (commentPos != std::string::npos)
            {
                line = line.substr(0, commentPos);
            }

            line = Trim(line);
            if (line.empty())
            {
                continue;
            }

            const auto delimiterPos = line.find('=');
            if (delimiterPos == std::string::npos)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Line " << lineNumber << " missing '=' : " << line << "\n";
                DebugTrace(oss.str());
                parseError = true;
                continue;
            }

            const std::string key = Trim(line.substr(0, delimiterPos));
            const std::string rawValue = Trim(line.substr(delimiterPos + 1));

            size_t platformIndex = 0;
            if (TryParsePlatformKey(key, "center", platformIndex))
            {
                auto ensureOverride = [&platformOverrides](size_t index)
                {
                    if (index >= platformOverrides.size())
                    {
                        platformOverrides.resize(index + 1);
                    }
                };
                ensureOverride(platformIndex);

                bool error = false;
                auto parsed = ParseFloatList(rawValue, &error);
                if (error || parsed.size() < 3)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "platform" << platformIndex
                        << ".center on line " << lineNumber
                        << " expected at least three comma-separated floats. Raw='" << rawValue << "'\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
                else
                {
                    platformOverrides[platformIndex].descriptor.center = float3{ parsed[0], parsed[1], parsed[2] };
                    platformOverrides[platformIndex].centerProvided = true;
                }
            }
            else if (TryParsePlatformKey(key, "scale", platformIndex))
            {
                auto ensureOverride = [&platformOverrides](size_t index)
                {
                    if (index >= platformOverrides.size())
                    {
                        platformOverrides.resize(index + 1);
                    }
                };
                ensureOverride(platformIndex);

                bool error = false;
                auto parsed = ParseFloatList(rawValue, &error);
                if (error || parsed.size() < 3)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "platform" << platformIndex
                        << ".scale on line " << lineNumber
                        << " expected at least three comma-separated floats. Raw='" << rawValue << "'\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
                else
                {
                    platformOverrides[platformIndex].descriptor.scale = float3{ parsed[0], parsed[1], parsed[2] };
                    platformOverrides[platformIndex].scaleProvided = true;
                }
            }
            else if (TryParsePlatformKey(key, "depth", platformIndex))
            {
                auto ensureOverride = [&platformOverrides](size_t index)
                {
                    if (index >= platformOverrides.size())
                    {
                        platformOverrides.resize(index + 1);
                    }
                };
                ensureOverride(platformIndex);

                try
                {
                    platformOverrides[platformIndex].descriptor.depth = std::stof(rawValue);
                    platformOverrides[platformIndex].depthProvided = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse platform" << platformIndex
                        << ".depth on line " << lineNumber << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (TryParsePlatformKey(key, "widthFraction", platformIndex))
            {
                auto ensureOverride = [&platformOverrides](size_t index)
                {
                    if (index >= platformOverrides.size())
                    {
                        platformOverrides.resize(index + 1);
                    }
                };
                ensureOverride(platformIndex);

                try
                {
                    platformOverrides[platformIndex].descriptor.widthFraction = std::stof(rawValue);
                    platformOverrides[platformIndex].widthProvided = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse platform" << platformIndex
                        << ".widthFraction on line " << lineNumber << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (TryParsePlatformKey(key, "heightFraction", platformIndex))
            {
                auto ensureOverride = [&platformOverrides](size_t index)
                {
                    if (index >= platformOverrides.size())
                    {
                        platformOverrides.resize(index + 1);
                    }
                };
                ensureOverride(platformIndex);

                try
                {
                    platformOverrides[platformIndex].descriptor.heightFraction = std::stof(rawValue);
                    platformOverrides[platformIndex].heightProvided = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse platform" << platformIndex
                        << ".heightFraction on line " << lineNumber << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "planeZ")
            {
                try
                {
                    planeZ = std::stof(rawValue);
                    planeZSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse planeZ on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "phaseThresholds")
            {
                bool error = false;
                auto parsed = ParseFloatList(rawValue, &error);
                if (error)
                {
                    parseError = true;
                }
                else
                {
                    thresholds.insert(thresholds.end(), parsed.begin(), parsed.end());
                }
            }
            else if (key == "spawnPoint")
            {
                if (!TryParseSpawnPoint(rawValue, lineNumber, spawnPoints))
                {
                    parseError = true;
                }
            }
            else if (key == "spawnPoints")
            {
                size_t beforeCount = spawnPoints.size();
                if (!TryParseSpawnPoint(rawValue, lineNumber, spawnPoints))
                {
                    parseError = true;
                }
                else if (spawnPoints.size() - beforeCount != 2)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "spawnPoints on line " << lineNumber
                        << " expected exactly two spawn anchors but found "
                        << (spawnPoints.size() - beforeCount) << ". Raw='" << rawValue << "'\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "phaseRewardMultipliers")
            {
                bool error = false;
                auto parsed = ParseFloatList(rawValue, &error);
                if (error)
                {
                    parseError = true;
                }
                else
                {
                    rewardMultipliers.insert(rewardMultipliers.end(), parsed.begin(), parsed.end());
                }
            }
            else if (key == "platformCenterY")
            {
                try
                {
                    platformCenterY = std::stof(rawValue);
                    platformCenterSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse platformCenterY on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "platformWidthFraction")
            {
                try
                {
                    platformWidthFraction = std::stof(rawValue);
                    platformWidthFractionSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse platformWidthFraction on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "platformHeightFraction")
            {
                try
                {
                    platformHeightFraction = std::stof(rawValue);
                    platformHeightFractionSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse platformHeightFraction on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "platformDepth")
            {
                try
                {
                    platformDepth = std::stof(rawValue);
                    platformDepthSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse platformDepth on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "platformScale")
            {
                bool error = false;
                auto parsed = ParseFloatList(rawValue, &error);
                if (error || parsed.size() < 3)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "platformScale on line " << lineNumber
                        << " must contain three floats. Raw='" << rawValue << "'\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
                else
                {
                    platformScale = float3{ parsed[0], parsed[1], parsed[2] };
                    platformScaleSet = true;
                }
            }
            else if (key == "playerMoveSpeed")
            {
                try
                {
                    playerMoveSpeed = std::stof(rawValue);
                    playerMoveSpeedSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse playerMoveSpeed on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "guidelineHeight")
            {
                try
                {
                    guidelineHeight = std::stof(rawValue);
                    guidelineHeightSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse guidelineHeight on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "heightBonusThreshold")
            {
                try
                {
                    heightBonusThreshold = std::stof(rawValue);
                    heightBonusThresholdSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse heightBonusThreshold on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "heightBonusSeconds")
            {
                try
                {
                    heightBonusSeconds = std::stof(rawValue);
                    heightBonusSecondsSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse heightBonusSeconds on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "backgroundTexture")
            {
                if (rawValue.empty())
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "backgroundTexture on line " << lineNumber
                        << " was empty; background rendering disabled.\n";
                    DebugTrace(oss.str());
                    backgroundTexturePath.clear();
                    backgroundTextureSet = true;
                }
                else
                {
                    backgroundTexturePath = rawValue;
                    backgroundTextureSet = true;
                }
            }
            else
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Unknown key '" << key << "' on line " << lineNumber << "\n";
                DebugTrace(oss.str());
            }
        }

        if (!planeZSet)
        {
            planeZ = kDefaultPlaneZ;
            std::ostringstream oss;
            oss << kLogPrefix << "planeZ not specified; defaulting to " << planeZ << "\n";
            DebugTrace(oss.str());
        }

        if (!platformCenterSet)
        {
            platformCenterY = kDefaultPlatformCenterY;
            std::ostringstream oss;
            oss << kLogPrefix << "platformCenterY not specified; defaulting to " << platformCenterY << "\n";
            DebugTrace(oss.str());
        }

        if (!platformScaleSet)
        {
            platformScale = kDefaultPlatformScale;
            std::ostringstream oss;
            oss << kLogPrefix << "platformScale not specified; using default ("
                << platformScale.x << ',' << platformScale.y << ',' << platformScale.z << ").\n";
            DebugTrace(oss.str());
        }

        if (!playerMoveSpeedSet)
        {
            playerMoveSpeed = kDefaultPlayerMoveSpeed;
            std::ostringstream oss;
            oss << kLogPrefix << "playerMoveSpeed not specified; defaulting to " << playerMoveSpeed << "\n";
            DebugTrace(oss.str());
        }

        if (!guidelineHeightSet)
        {
            guidelineHeight = kDefaultGuidelineHeight;
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineHeight not specified; defaulting to " << guidelineHeight << "\n";
            DebugTrace(oss.str());
        }

        if (!heightBonusThresholdSet)
        {
            heightBonusThreshold = kDefaultHeightBonusThreshold;
            std::ostringstream oss;
            oss << kLogPrefix << "heightBonusThreshold not specified; defaulting to " << heightBonusThreshold << "\n";
            DebugTrace(oss.str());
        }

        if (!heightBonusSecondsSet)
        {
            heightBonusSeconds = kDefaultHeightBonusSeconds;
            std::ostringstream oss;
            oss << kLogPrefix << "heightBonusSeconds not specified; defaulting to " << heightBonusSeconds << "\n";
            DebugTrace(oss.str());
        }

        if (!backgroundTextureSet)
        {
            backgroundTexturePath = kDefaultBackgroundTexturePath;
            std::ostringstream oss;
            oss << kLogPrefix << "backgroundTexture not specified; defaulting to '"
                << backgroundTexturePath << "'.\n";
            DebugTrace(oss.str());
        }

        if (!platformWidthFractionSet)
        {
            platformWidthFraction = kDefaultPlatformWidthFraction;
            std::ostringstream oss;
            oss << kLogPrefix << "platformWidthFraction not specified; defaulting to "
                << platformWidthFraction << "\n";
            DebugTrace(oss.str());
        }

        if (!platformHeightFractionSet)
        {
            platformHeightFraction = kDefaultPlatformHeightFraction;
            std::ostringstream oss;
            oss << kLogPrefix << "platformHeightFraction not specified; defaulting to "
                << platformHeightFraction << "\n";
            DebugTrace(oss.str());
        }

        if (!platformDepthSet)
        {
            platformDepth = kDefaultPlatformDepth;
            std::ostringstream oss;
            oss << kLogPrefix << "platformDepth not specified; defaulting to " << platformDepth << "\n";
            DebugTrace(oss.str());
        }

        auto sanitizeFraction = [](float value, float defaultValue, const char* name)
        {
            float sanitized = value;
            if (!std::isfinite(sanitized) || sanitized <= 0.0f)
            {
                sanitized = defaultValue;
            }
            bool treatedAsPercent = false;
            if (sanitized > 1.0f)
            {
                sanitized *= 0.01f;
                treatedAsPercent = true;
            }
            sanitized = std::clamp(sanitized, 0.01f, 1.0f);

            if (!std::isfinite(value) || value <= 0.0f)
            {
                std::ostringstream oss;
                oss << kLogPrefix << name << " invalid; using default " << defaultValue << "\n";
                DebugTrace(oss.str());
            }
            else if (treatedAsPercent)
            {
                std::ostringstream oss;
                oss << kLogPrefix << name << " treated as percent; normalized to " << sanitized << "\n";
                DebugTrace(oss.str());
            }
            else if (sanitized != value)
            {
                std::ostringstream oss;
                oss << kLogPrefix << name << " clamped to " << sanitized << " (from " << value << ")\n";
                DebugTrace(oss.str());
            }

            return sanitized;
        };

        platformWidthFraction = sanitizeFraction(platformWidthFraction, kDefaultPlatformWidthFraction, "platformWidthFraction");
        platformHeightFraction = sanitizeFraction(platformHeightFraction, kDefaultPlatformHeightFraction, "platformHeightFraction");

        auto sanitizeDepth = [](float value, float defaultValue, const std::string& name)
        {
            float sanitized = value;
            if (!std::isfinite(sanitized) || sanitized == 0.0f)
            {
                std::ostringstream oss;
                oss << kLogPrefix << name << " value " << sanitized
                    << " invalid; using default " << defaultValue << "\n";
                DebugTrace(oss.str());
                sanitized = defaultValue;
            }
            sanitized = std::max(0.01f, std::fabs(sanitized));
            return sanitized;
        };

        auto sanitizeScaleVector = [](const float3& value, const std::string& name)
        {
            auto sanitizeScale = [](float component)
            {
                return std::max(0.01f, std::fabs(component));
            };

            float3 sanitized = value;
            const float3 raw = sanitized;
            sanitized.x = sanitizeScale(sanitized.x);
            sanitized.y = sanitizeScale(sanitized.y);
            sanitized.z = sanitizeScale(sanitized.z);
            if (sanitized.x != raw.x || sanitized.y != raw.y || sanitized.z != raw.z)
            {
                std::ostringstream oss;
                oss << kLogPrefix << name << " values sanitized from ("
                    << raw.x << ',' << raw.y << ',' << raw.z << ") to ("
                    << sanitized.x << ',' << sanitized.y << ',' << sanitized.z << ").\n";
                DebugTrace(oss.str());
            }
            return sanitized;
        };

        auto sanitizeCenterComponent = [](float value, float defaultValue, const std::string& label)
        {
            if (!std::isfinite(value))
            {
                std::ostringstream oss;
                oss << kLogPrefix << label << " invalid; defaulting to " << defaultValue << "\n";
                DebugTrace(oss.str());
                return defaultValue;
            }
            return value;
        };

        platformDepth = sanitizeDepth(platformDepth, kDefaultPlatformDepth, "platformDepth");
        platformScale = sanitizeScaleVector(platformScale, "platformScale");

        auto buildDefaultPlatform = [&]()
        {
            PlatformDescriptor descriptor{};
            descriptor.center = float3{ 0.0f, platformCenterY, planeZ };
            descriptor.scale = platformScale;
            descriptor.depth = platformDepth;
            descriptor.widthFraction = platformWidthFraction;
            descriptor.heightFraction = platformHeightFraction;
            return descriptor;
        };

        auto sanitizePlatformDescriptor = [&](PlatformDescriptor descriptor, std::size_t index)
        {
            const std::string labelPrefix = std::string("platform") + std::to_string(index);
            descriptor.center.x = sanitizeCenterComponent(descriptor.center.x, 0.0f, labelPrefix + ".centerX");
            descriptor.center.y = sanitizeCenterComponent(descriptor.center.y, platformCenterY, labelPrefix + ".centerY");
            descriptor.center.z = sanitizeCenterComponent(descriptor.center.z, planeZ, labelPrefix + ".centerZ");

            descriptor.scale = sanitizeScaleVector(descriptor.scale, labelPrefix + ".scale");
            descriptor.depth = sanitizeDepth(descriptor.depth, platformDepth, labelPrefix + ".depth");

            const std::string widthLabel = labelPrefix + ".widthFraction";
            descriptor.widthFraction = sanitizeFraction(descriptor.widthFraction, platformWidthFraction, widthLabel.c_str());
            const std::string heightLabel = labelPrefix + ".heightFraction";
            descriptor.heightFraction = sanitizeFraction(descriptor.heightFraction, platformHeightFraction, heightLabel.c_str());
            return descriptor;
        };

        std::vector<PlatformDescriptor> platformDescriptors;
        if (platformOverrides.empty())
        {
            platformDescriptors.push_back(sanitizePlatformDescriptor(buildDefaultPlatform(), 0));
        }
        else
        {
            for (std::size_t i = 0; i < platformOverrides.size(); ++i)
            {
                PlatformDescriptor descriptor = buildDefaultPlatform();
                const PlatformOverride& override = platformOverrides[i];
                if (override.centerProvided)
                {
                    descriptor.center = override.descriptor.center;
                }
                if (override.scaleProvided)
                {
                    descriptor.scale = override.descriptor.scale;
                }
                if (override.depthProvided)
                {
                    descriptor.depth = override.descriptor.depth;
                }
                if (override.widthProvided)
                {
                    descriptor.widthFraction = override.descriptor.widthFraction;
                }
                if (override.heightProvided)
                {
                    descriptor.heightFraction = override.descriptor.heightFraction;
                }

                platformDescriptors.push_back(sanitizePlatformDescriptor(descriptor, i));
            }
        }

        if (thresholds.empty())
        {
            thresholds.assign(std::begin(kDefaultThresholds), std::end(kDefaultThresholds));
            std::ostringstream oss;
            oss << kLogPrefix << "No phaseThresholds provided; using defaults.\n";
            DebugTrace(oss.str());
        }

        std::sort(thresholds.begin(), thresholds.end());
        thresholds.erase(
            std::remove_if(thresholds.begin(), thresholds.end(), [](float value)
                {
                    return !std::isfinite(value) || value <= 0.0f;
                }),
            thresholds.end());

        thresholds.erase(std::unique(thresholds.begin(), thresholds.end()), thresholds.end());

        if (thresholds.empty())
        {
            thresholds.assign(std::begin(kDefaultThresholds), std::end(kDefaultThresholds));
            std::ostringstream oss;
            oss << kLogPrefix << "All thresholds were invalid; falling back to defaults.\n";
            DebugTrace(oss.str());
        }

        if (spawnPoints.empty())
        {
            spawnPoints.assign(std::begin(kDefaultSpawnPoints), std::end(kDefaultSpawnPoints));
            std::ostringstream oss;
            oss << kLogPrefix << "No spawnPoint entries found; using defaults.\n";
            DebugTrace(oss.str());
        }

        if (!std::isfinite(playerMoveSpeed) || playerMoveSpeed <= 0.0f)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "playerMoveSpeed value " << playerMoveSpeed
                << " invalid; falling back to default " << kDefaultPlayerMoveSpeed << "\n";
            DebugTrace(oss.str());
            playerMoveSpeed = kDefaultPlayerMoveSpeed;
        }

        if (!std::isfinite(guidelineHeight) || guidelineHeight <= 0.0f)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "guidelineHeight value " << guidelineHeight
                << " invalid; using default " << kDefaultGuidelineHeight << "\n";
            DebugTrace(oss.str());
            guidelineHeight = kDefaultGuidelineHeight;
        }

        if (!std::isfinite(heightBonusThreshold) || heightBonusThreshold <= 0.0f)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "heightBonusThreshold value " << heightBonusThreshold
                << " invalid; using default " << kDefaultHeightBonusThreshold << "\n";
            DebugTrace(oss.str());
            heightBonusThreshold = kDefaultHeightBonusThreshold;
        }

        if (!std::isfinite(heightBonusSeconds))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "heightBonusSeconds value " << heightBonusSeconds
                << " invalid; using default " << kDefaultHeightBonusSeconds << "\n";
            DebugTrace(oss.str());
            heightBonusSeconds = kDefaultHeightBonusSeconds;
        }
        else if (heightBonusSeconds <= 0.0f)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "heightBonusSeconds <= 0. Bonus events disabled.\n";
            DebugTrace(oss.str());
            heightBonusSeconds = 0.0f;
        }

        const std::size_t requiredMultiplierCount = thresholds.size() + 1;
        EnsureRewardMultiplierCoverage(rewardMultipliers, requiredMultiplierCount);

        m_planeZ = planeZ;
        m_phaseThresholds = thresholds;
        m_phaseRewardMultipliers = rewardMultipliers;
        m_spawnPoints = spawnPoints;
        m_platforms = std::move(platformDescriptors);
        m_playerMoveSpeed = playerMoveSpeed;
        m_guidelineHeight = guidelineHeight;
        m_heightBonusThreshold = heightBonusThreshold;
        m_heightBonusSeconds = heightBonusSeconds;
        m_backgroundTexturePath = backgroundTexturePath;
        m_sourcePath = path;
        m_loadedFromFile = !parseError;

        std::ostringstream oss;
        oss << kLogPrefix << "Loaded stage data from '" << path << "' (planeZ=" << m_planeZ
            << ", thresholds=" << m_phaseThresholds.size()
            << ", multipliers=" << m_phaseRewardMultipliers.size()
            << ", spawnPoints=" << m_spawnPoints.size()
            << ", platforms=" << m_platforms.size()
            << ", playerMoveSpeed=" << m_playerMoveSpeed
            << ", guidelineHeight=" << m_guidelineHeight
            << ", heightBonusThreshold=" << m_heightBonusThreshold
            << ", heightBonusSeconds=" << m_heightBonusSeconds
            << ", backgroundTexture='" << m_backgroundTexturePath << "'"
            << ", parseError=" << std::boolalpha << parseError << ").\n";
        DebugTrace(oss.str());

        for (std::size_t i = 0; i < m_platforms.size(); ++i)
        {
            LogPlatformSummary(i, m_platforms[i]);
        }

        return m_loadedFromFile;
    }

    const std::vector<float>& StageData::GetPhaseThresholds() const noexcept
    {
        return m_phaseThresholds;
    }

    const std::vector<float>& StageData::GetPhaseRewardMultipliers() const noexcept
    {
        return m_phaseRewardMultipliers;
    }

    const std::vector<float3>& StageData::GetSpawnPoints() const noexcept
    {
        return m_spawnPoints;
    }

    float StageData::GetPlaneZ() const noexcept
    {
        return m_planeZ;
    }

    const std::vector<PlatformDescriptor>& StageData::GetPlatforms() const noexcept
    {
        return m_platforms;
    }

    const PlatformDescriptor& StageData::GetPlatform(std::size_t index) const
    {
        if (index < m_platforms.size())
        {
            return m_platforms[index];
        }

        if (m_platforms.empty())
        {
            static PlatformDescriptor s_fallback{ float3{ 0.0f, kDefaultPlatformCenterY, kDefaultPlaneZ }, kDefaultPlatformScale, kDefaultPlatformDepth, kDefaultPlatformWidthFraction, kDefaultPlatformHeightFraction };
            return s_fallback;
        }

        return m_platforms.front();
    }

    std::size_t StageData::GetPlatformCount() const noexcept
    {
        return m_platforms.size();
    }

    const std::string& StageData::GetSourcePath() const noexcept
    {
        return m_sourcePath;
    }

    float StageData::GetPlayerMoveSpeed() const noexcept
    {
        return m_playerMoveSpeed;
    }

    float StageData::GetGuidelineHeight() const noexcept
    {
        return m_guidelineHeight;
    }

    const std::string& StageData::GetBackgroundTexturePath() const noexcept
    {
        return m_backgroundTexturePath;
    }

    float StageData::GetHeightBonusThreshold() const noexcept
    {
        return m_heightBonusThreshold;
    }

    float StageData::GetHeightBonusSeconds() const noexcept
    {
        return m_heightBonusSeconds;
    }

    void StageData::ApplyDefaults()
    {
        m_planeZ = kDefaultPlaneZ;
        m_phaseThresholds.assign(std::begin(kDefaultThresholds), std::end(kDefaultThresholds));
        m_phaseRewardMultipliers.assign(std::begin(kDefaultRewardMultipliers), std::end(kDefaultRewardMultipliers));
        EnsureRewardMultiplierCoverage(m_phaseRewardMultipliers, m_phaseThresholds.size() + 1);
        m_spawnPoints.assign(std::begin(kDefaultSpawnPoints), std::end(kDefaultSpawnPoints));
        m_platforms.clear();
        PlatformDescriptor descriptor{};
        descriptor.center = float3{ 0.0f, kDefaultPlatformCenterY, kDefaultPlaneZ };
        descriptor.scale = kDefaultPlatformScale;
        descriptor.depth = kDefaultPlatformDepth;
        descriptor.widthFraction = kDefaultPlatformWidthFraction;
        descriptor.heightFraction = kDefaultPlatformHeightFraction;
        m_platforms.push_back(descriptor);
        m_playerMoveSpeed = kDefaultPlayerMoveSpeed;
        m_guidelineHeight = kDefaultGuidelineHeight;
        m_backgroundTexturePath = kDefaultBackgroundTexturePath;
        m_heightBonusThreshold = kDefaultHeightBonusThreshold;
        m_heightBonusSeconds = kDefaultHeightBonusSeconds;
        m_loadedFromFile = false;
        m_sourcePath.clear();
        LogSummary("Defaults", "applied");
    }

    void StageData::LogSummary(const char* prefix, const std::string& detail) const
    {
        std::ostringstream oss;
        oss << kLogPrefix << prefix << " planeZ=" << m_planeZ
            << " thresholds=" << m_phaseThresholds.size()
            << " multipliers=" << m_phaseRewardMultipliers.size()
            << " spawnPoints=" << m_spawnPoints.size()
            << " platforms=" << m_platforms.size()
            << " playerMoveSpeed=" << m_playerMoveSpeed
            << " guidelineHeight=" << m_guidelineHeight
            << " heightBonusThreshold=" << m_heightBonusThreshold
            << " heightBonusSeconds=" << m_heightBonusSeconds
            << " backgroundTexture='" << m_backgroundTexturePath << "'"
            << " detail=" << detail << "\n";
        DebugTrace(oss.str());

        for (std::size_t i = 0; i < m_platforms.size(); ++i)
        {
            LogPlatformSummary(i, m_platforms[i]);
        }
    }

    void StageData::LogPlatformSummary(std::size_t index, const PlatformDescriptor& descriptor) const
    {
        std::ostringstream oss;
        oss << kLogPrefix << " platform[" << index << "] center=(" << descriptor.center.x << ',' << descriptor.center.y
            << ',' << descriptor.center.z << ") scale=(" << descriptor.scale.x << ',' << descriptor.scale.y << ','
            << descriptor.scale.z << ") depth=" << descriptor.depth
            << " widthFraction=" << descriptor.widthFraction
            << " heightFraction=" << descriptor.heightFraction << "\n";
        DebugTrace(oss.str());
    }

    void StageData::EnsureRewardMultiplierCoverage(std::vector<float>& multipliers, std::size_t requiredCount) const
    {
        if (requiredCount == 0)
        {
            requiredCount = 1;
        }

        const auto beforeFilter = multipliers.size();
        multipliers.erase(
            std::remove_if(multipliers.begin(), multipliers.end(), [](float value)
                {
                    return !std::isfinite(value) || value <= 0.0f;
                }),
            multipliers.end());
        if (beforeFilter != multipliers.size())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Filtered " << (beforeFilter - multipliers.size())
                << " invalid reward multipliers." << "\n";
            DebugTrace(oss.str());
        }

        if (multipliers.empty())
        {
            multipliers.assign(std::begin(kDefaultRewardMultipliers), std::end(kDefaultRewardMultipliers));
            std::ostringstream oss;
            oss << kLogPrefix << "No phaseRewardMultipliers provided; using defaults." << "\n";
            DebugTrace(oss.str());
        }

        if (multipliers.size() < requiredCount)
        {
            const float fillValue = multipliers.back();
            const auto previousSize = multipliers.size();
            multipliers.resize(requiredCount, fillValue);
            std::ostringstream oss;
            oss << kLogPrefix << "Extended reward multipliers from " << previousSize
                << " to " << multipliers.size() << " entries using fillValue=" << fillValue << ".\n";
            DebugTrace(oss.str());
        }
        else if (multipliers.size() > requiredCount)
        {
            multipliers.resize(requiredCount);
            std::ostringstream oss;
            oss << kLogPrefix << "Truncated reward multipliers to match phase count (" << requiredCount << ").\n";
            DebugTrace(oss.str());
        }
    }
}
