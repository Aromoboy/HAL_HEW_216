#include "Stage/StageData.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <DirectXMath.h>
#include <fstream>
#include <cstring>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace
{
    void DebugTrace(const std::string& message)
    {
        (void)message;
    }

    constexpr const char* kLogPrefix = "[StageData] ";
    constexpr float kDefaultPlaneZ = -5.0f;
    constexpr float kDefaultPlatformCenterY = 0.0f;
    constexpr float kDefaultPlatformWidthFraction = 0.20f;
    constexpr float kDefaultPlatformHeightFraction = 0.05f;
    constexpr float kPlatformGridWidthCells = 40.0f;
    constexpr float kPlatformGridHeightCells = 30.0f;
    constexpr float kDefaultPlatformDepth = 4.0f;
    const float3 kDefaultPlatformScale{ 12.0f, 2.0f, 4.0f };
    const DirectX::XMFLOAT4 kDefaultPlatformPrimaryColor{ 0.305f, 0.173f, 0.125f, 1.0f };
    const DirectX::XMFLOAT4 kDefaultPlatformSecondaryColor{ 0.180f, 0.258f, 0.329f, 1.0f };
    constexpr float kDefaultCellSizeWorld = 1.0f;
    constexpr float kDefaultHorizontalSpeedCellsPerSec = 10.0f;
    constexpr float kDefaultVerticalSpeedCellsPerSec = 12.0f;
    constexpr float kDefaultPickupBoxWidthCells = 5.0f;
    constexpr float kDefaultPickupBoxHeightCells = 5.0f;
    constexpr float kDefaultPickupMaxDistance = 0.5f;
    constexpr float kDefaultPlayerMoveSpeed = kDefaultHorizontalSpeedCellsPerSec * kDefaultCellSizeWorld;
    constexpr float kDefaultGuidelineHeight = 20.0f;
    constexpr float kDefaultHeightBonusThreshold = kDefaultGuidelineHeight;
    constexpr float kDefaultHeightBonusSeconds = 5.0f;
    constexpr const char* kDefaultBackgroundTexturePath = "Assets/Texture/Title.jpg";
    constexpr float kDefaultOsaraLeftOffsetX = -3.0f;
    constexpr float kDefaultOsaraRightOffsetX = 3.0f;
    constexpr float kDefaultOsaraOffsetY = 2.0f;
    constexpr float kDefaultOsaraSpawnOffsetY = 2.0f;
    constexpr std::size_t kRequiredSpawnAnchors = 2;
    constexpr std::size_t kSpawnCoordinatesPerAnchor = 3;
    constexpr float kSpawnPlaneTolerance = 0.01f;

    const float3 kDefaultSpawnPoints[] =
    {
        { -2.5f, 0.0f, kDefaultPlaneZ },
        { 2.5f, 0.0f, kDefaultPlaneZ },
    };

    const float kDefaultThresholds[] = { 5.0f, 15.0f, 30.0f };
    const float kDefaultRewardMultipliers[] = { 1.0f };
    //ミニフェーズ
    const float kDefaultMiniThresholds[] = { 3.0f, 7.0f, 16.0f };
    const float kDefaultMiniRewardMultipliers[] = { 1.0f };

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

    DirectX::XMFLOAT4 SanitizeColor(const DirectX::XMFLOAT4& color, const DirectX::XMFLOAT4& fallback, const std::string& label)
    {
        auto clampComponent = [](float value)
        {
            return std::clamp(value, 0.0f, 1.0f);
        };

        if (!std::isfinite(color.x) || !std::isfinite(color.y) || !std::isfinite(color.z) || !std::isfinite(color.w))
        {
            std::ostringstream oss;
            oss << kLogPrefix << label << " invalid; using fallback color.\n";
            DebugTrace(oss.str());
            return fallback;
        }

        DirectX::XMFLOAT4 sanitized{ clampComponent(color.x), clampComponent(color.y), clampComponent(color.z), clampComponent(color.w) };
        if (sanitized.x != color.x || sanitized.y != color.y || sanitized.z != color.z || sanitized.w != color.w)
        {
            std::ostringstream oss;
            oss << kLogPrefix << label << " clamped to (" << sanitized.x << ',' << sanitized.y << ',' << sanitized.z << ',' << sanitized.w << ")\n";
            DebugTrace(oss.str());
        }

        return sanitized;
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

        if (parsed.size() % kSpawnCoordinatesPerAnchor != 0)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawn point on line " << lineNumber
                << " expected groups of three floats but found " << parsed.size()
                << " values. Raw='" << rawValue << "'\n";
            DebugTrace(oss.str());
            return false;
        }

        for (size_t i = 0; i + 2 < parsed.size(); i += kSpawnCoordinatesPerAnchor)
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
        bool colorProvided = false;
    };
}

namespace Stage
{
    StageData::StageData()
        : m_planeZ(kDefaultPlaneZ)
        , m_loadedFromFile(false)
        , m_cellSizeWorld(kDefaultCellSizeWorld)
        , m_playerHorizontalSpeedCellsPerSec(kDefaultHorizontalSpeedCellsPerSec)
        , m_playerVerticalSpeedCellsPerSec(kDefaultVerticalSpeedCellsPerSec)
        , m_playerHorizontalSpeedWorld(kDefaultPlayerMoveSpeed)
        , m_playerVerticalSpeedWorld(kDefaultVerticalSpeedCellsPerSec * kDefaultCellSizeWorld)
        , m_pickupBoxWidthCells(kDefaultPickupBoxWidthCells)
        , m_pickupBoxHeightCells(kDefaultPickupBoxHeightCells)
        , m_pickupBoxWidthWorld(kDefaultPickupBoxWidthCells * kDefaultCellSizeWorld)
        , m_pickupBoxHeightWorld(kDefaultPickupBoxHeightCells * kDefaultCellSizeWorld)
        , m_pickupMaxDistance(kDefaultPickupMaxDistance)
        , m_playerMoveSpeed(kDefaultPlayerMoveSpeed)
        , m_guidelineHeight(kDefaultGuidelineHeight)
        , m_backgroundTexturePath(kDefaultBackgroundTexturePath)
        , m_heightBonusThreshold(kDefaultHeightBonusThreshold)
        , m_heightBonusSeconds(kDefaultHeightBonusSeconds)
        , m_osaraLeftOffsetX(kDefaultOsaraLeftOffsetX)
        , m_osaraRightOffsetX(kDefaultOsaraRightOffsetX)
        , m_osaraOffsetY(kDefaultOsaraOffsetY)
        , m_osaraSpawnOffsetY(kDefaultOsaraSpawnOffsetY)
        , m_osaraOffsetsConfigured(false)
        , m_spawnPointsSuppressed(false)
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
        std::vector<float> minithresholds; //ミニフェーズ
        std::vector<float3> spawnPoints;
        std::vector<float> rewardMultipliers; //ミニフェーズ
        std::vector<float> minirewardMultipliers;
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
        float cellSizeWorld = kDefaultCellSizeWorld;
        bool cellSizeWorldSet = false;
        float horizontalSpeedCellsPerSec = kDefaultHorizontalSpeedCellsPerSec;
        bool horizontalSpeedSet = false;
        float verticalSpeedCellsPerSec = kDefaultVerticalSpeedCellsPerSec;
        bool verticalSpeedSet = false;
        float pickupBoxWidthCells = kDefaultPickupBoxWidthCells;
        bool pickupBoxWidthSet = false;
        float pickupBoxHeightCells = kDefaultPickupBoxHeightCells;
        bool pickupBoxHeightSet = false;
        float pickupMaxDistance = kDefaultPickupMaxDistance;
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
        float osaraLeftOffsetX = kDefaultOsaraLeftOffsetX;
        bool osaraLeftOffsetSet = false;
        float osaraRightOffsetX = kDefaultOsaraRightOffsetX;
        bool osaraRightOffsetSet = false;
        float osaraOffsetY = kDefaultOsaraOffsetY;
        bool osaraOffsetYSet = false;
        float osaraSpawnOffsetY = kDefaultOsaraSpawnOffsetY;
        bool osaraSpawnOffsetYSet = false;
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
            else if (TryParsePlatformKey(key, "color", platformIndex))
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
                        << ".color on line " << lineNumber
                        << " expected at least three comma-separated floats. Raw='" << rawValue << "'\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
                else
                {
                    DirectX::XMFLOAT4 color{ parsed[0], parsed[1], parsed[2], 1.0f };
                    if (parsed.size() >= 4)
                    {
                        color.w = parsed[3];
                    }
                    platformOverrides[platformIndex].descriptor.color = color;
                    platformOverrides[platformIndex].colorProvided = true;
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
            //ミニフェーズ
            else if (key == "phaseMiniThresholds")
            {
                bool error = false;
                auto parsed = ParseFloatList(rawValue, &error);
                if (error)
                {
                    parseError = true;
                }
                else
                {
                    minithresholds.insert(minithresholds.end(), parsed.begin(), parsed.end());
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
                else
                {
                    const size_t added = spawnPoints.size() - beforeCount;
                    std::ostringstream oss;
                    oss << kLogPrefix << "spawnPoints on line " << lineNumber
                        << " parsed " << added << " anchor" << (added == 1 ? "" : "s")
                        << " (" << spawnPoints.size() << " total). Each anchor requires"
                        << " three coordinates. Raw='" << rawValue << "'\n";
                    DebugTrace(oss.str());
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
            //ミニフェーズ
            else if (key == "phaseMiniRewardMultipliers")
            {
                bool error = false;
                auto parsed = ParseFloatList(rawValue, &error);
                if (error)
                {
                    parseError = true;
                }
                else
                {
                    minirewardMultipliers.insert(minirewardMultipliers.end(), parsed.begin(), parsed.end());
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
            else if (key == "cellSizeWorld")
            {
                try
                {
                    cellSizeWorld = std::stof(rawValue);
                    cellSizeWorldSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse cellSizeWorld on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "horizontalSpeedCellsPerSec")
            {
                try
                {
                    horizontalSpeedCellsPerSec = std::stof(rawValue);
                    horizontalSpeedSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse horizontalSpeedCellsPerSec on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "verticalSpeedCellsPerSec")
            {
                try
                {
                    verticalSpeedCellsPerSec = std::stof(rawValue);
                    verticalSpeedSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse verticalSpeedCellsPerSec on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "pickupBoxWidthCells")
            {
                try
                {
                    pickupBoxWidthCells = std::stof(rawValue);
                    pickupBoxWidthSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse pickupBoxWidthCells on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "pickupBoxHeightCells")
            {
                try
                {
                    pickupBoxHeightCells = std::stof(rawValue);
                    pickupBoxHeightSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse pickupBoxHeightCells on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "pickupMaxDistance")
            {
                try
                {
                    pickupMaxDistance = std::stof(rawValue);
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse pickupMaxDistance on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
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
            else if (key == "osara_left_offset_x")
            {
                try
                {
                    osaraLeftOffsetX = std::stof(rawValue);
                    osaraLeftOffsetSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse osara_left_offset_x on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "osara_right_offset_x")
            {
                try
                {
                    osaraRightOffsetX = std::stof(rawValue);
                    osaraRightOffsetSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse osara_right_offset_x on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "osara_offset_y")
            {
                try
                {
                    osaraOffsetY = std::stof(rawValue);
                    osaraOffsetYSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse osara_offset_y on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
            }
            else if (key == "osara_spawn_offset_y")
            {
                try
                {
                    osaraSpawnOffsetY = std::stof(rawValue);
                    osaraSpawnOffsetYSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse osara_spawn_offset_y on line " << lineNumber
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

        if (!playerMoveSpeedSet && !horizontalSpeedSet)
        {
            playerMoveSpeed = kDefaultPlayerMoveSpeed;
            std::ostringstream oss;
            oss << kLogPrefix << "playerMoveSpeed not specified; defaulting to " << playerMoveSpeed
                << " (legacy fallback).\n";
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

        auto assignDefaultOffset = [](bool provided, float& value, float defaultValue, const char* label)
        {
            if (provided)
            {
                return;
            }

            value = defaultValue;
            std::ostringstream oss;
            oss << kLogPrefix << label << " not specified; defaulting to " << value << "\n";
            DebugTrace(oss.str());
        };

        assignDefaultOffset(osaraLeftOffsetSet, osaraLeftOffsetX, kDefaultOsaraLeftOffsetX, "osara_left_offset_x");
        assignDefaultOffset(osaraRightOffsetSet, osaraRightOffsetX, kDefaultOsaraRightOffsetX, "osara_right_offset_x");
        assignDefaultOffset(osaraOffsetYSet, osaraOffsetY, kDefaultOsaraOffsetY, "osara_offset_y");
        assignDefaultOffset(osaraSpawnOffsetYSet, osaraSpawnOffsetY, kDefaultOsaraSpawnOffsetY, "osara_spawn_offset_y");

        const bool osaraOffsetsConfigured = osaraLeftOffsetSet || osaraRightOffsetSet || osaraOffsetYSet || osaraSpawnOffsetYSet;

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
            sanitized = std::max(0.01f, static_cast<float>(std::fabs(sanitized)));
            return sanitized;
        };

        auto sanitizeScaleVector = [](const float3& value, const std::string& name)
        {
            auto sanitizeScale = [](float component)
            {
                return std::max(0.01f, static_cast<float>(std::fabs(component)));
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

        auto defaultColorForIndex = [](std::size_t index)
        {
            return (index % 2 == 0) ? kDefaultPlatformPrimaryColor : kDefaultPlatformSecondaryColor;
        };

        auto buildDefaultPlatform = [&](std::size_t index)
        {
            PlatformDescriptor descriptor{};
            descriptor.center = float3{ 0.0f, platformCenterY, planeZ };
            descriptor.scale = platformScale;
            descriptor.depth = platformDepth;
            descriptor.widthFraction = platformWidthFraction;
            descriptor.heightFraction = platformHeightFraction;
            descriptor.color = defaultColorForIndex(index);
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
            descriptor.color = SanitizeColor(descriptor.color, defaultColorForIndex(index), labelPrefix + ".color");
            return descriptor;
        };

        std::vector<PlatformDescriptor> platformDescriptors;
        if (platformOverrides.empty())
        {
            platformDescriptors.push_back(sanitizePlatformDescriptor(buildDefaultPlatform(0), 0));
        }
        else
        {
            for (std::size_t i = 0; i < platformOverrides.size(); ++i)
            {
                PlatformDescriptor descriptor = buildDefaultPlatform(i);
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
                if (override.colorProvided)
                {
                    descriptor.color = override.descriptor.color;
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
        //ミニフェーズ
        if (minithresholds.empty())
        {
            minithresholds.assign(std::begin(kDefaultMiniThresholds), std::end(kDefaultMiniThresholds));
            std::ostringstream oss;
            oss << kLogPrefix << "No phaseMiniThresholds provided; using defaults.\n";
            DebugTrace(oss.str());
        }

        std::sort(minithresholds.begin(), minithresholds.end());
        minithresholds.erase(
            std::remove_if(minithresholds.begin(), minithresholds.end(), [](float value)
                {
                    return !std::isfinite(value) || value <= 0.0f;
                }),
            minithresholds.end());

        minithresholds.erase(std::unique(minithresholds.begin(), minithresholds.end()), minithresholds.end());

        if (minithresholds.empty())
        {
            minithresholds.assign(std::begin(kDefaultMiniThresholds), std::end(kDefaultMiniThresholds));
            std::ostringstream oss;
            oss << kLogPrefix << "All minithresholds were invalid; falling back to defaults.\n";
            DebugTrace(oss.str());
        }

        const std::size_t parsedAnchorCount = spawnPoints.size();
        bool spawnPointsSuppressed = false;
        if (parsedAnchorCount > 0 && parsedAnchorCount < kRequiredSpawnAnchors)
        {
            parseError = true;
            spawnPoints.assign(std::begin(kDefaultSpawnPoints), std::end(kDefaultSpawnPoints));
            std::ostringstream oss;
            oss << kLogPrefix << "spawnPoints provided " << parsedAnchorCount
                << " anchor" << (parsedAnchorCount == 1 ? "" : "s")
                << "; expected at least " << kRequiredSpawnAnchors
                << ". Falling back to defaults and marking parse error.\n";
            DebugTrace(oss.str());
        }

        if (spawnPoints.empty())
        {
            if (osaraOffsetsConfigured)
            {
                spawnPointsSuppressed = true;
                std::ostringstream oss;
                oss << kLogPrefix << "No spawnPoint entries found; Osara offsets configured so spawnPoints will be suppressed"
                    << " (Osara anchors take precedence).\n";
                DebugTrace(oss.str());
            }
            else
            {
                spawnPoints.assign(std::begin(kDefaultSpawnPoints), std::end(kDefaultSpawnPoints));
                std::ostringstream oss;
                oss << kLogPrefix << "No spawnPoint entries found; using defaults.\n";
                DebugTrace(oss.str());
            }
        }

        const float osaraPlaneZ = platformDescriptors.empty() ? planeZ : platformDescriptors.front().center.z;

        if (!spawnPoints.empty())
        {
            const float spawnPlaneZ = spawnPoints.front().z;
            bool spawnPlaneConsistent = true;
            for (const auto& spawn : spawnPoints)
            {
                if (std::fabs(spawn.z - spawnPlaneZ) > kSpawnPlaneTolerance)
                {
                    spawnPlaneConsistent = false;
                    break;
                }
            }

            if (!spawnPlaneConsistent)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "spawnPoints contain multiple planeZ values; anchors may be misaligned.\n";
                DebugTrace(oss.str());
            }

            const float spawnPlaneDelta = std::fabs(spawnPlaneZ - planeZ);
            if (spawnPlaneDelta > kSpawnPlaneTolerance)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "spawnPoints planeZ=" << spawnPlaneZ
                    << " differs from configured planeZ=" << planeZ
                    << " by " << spawnPlaneDelta << "; verify alignment.\n";
                DebugTrace(oss.str());
            }

            if (osaraOffsetsConfigured)
            {
                const float osaraDelta = std::fabs(spawnPlaneZ - osaraPlaneZ);
                if (osaraDelta > kSpawnPlaneTolerance)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "spawnPoints planeZ=" << spawnPlaneZ
                        << " differs from Osara/platform planeZ=" << osaraPlaneZ
                        << " by " << osaraDelta << "; Osara anchors will override spawnPoints.\n";
                    DebugTrace(oss.str());
                }
            }
        }
        else if (osaraOffsetsConfigured && std::fabs(osaraPlaneZ - planeZ) > kSpawnPlaneTolerance)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Osara/platform planeZ=" << osaraPlaneZ
                << " differs from stage planeZ=" << planeZ
                << "; spawnPoints suppressed so Osara anchors must define the spawn plane.\n";
            DebugTrace(oss.str());
        }

        if (osaraOffsetsConfigured)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Osara anchors configured; spawnPoints "
                << (spawnPoints.empty() ? "suppressed" : "available as fallback")
                << ". Osara anchors will take precedence when present.\n";
            DebugTrace(oss.str());
        }

        auto sanitizePositive = [](float value, float fallback, const char* label)
        {
            if (std::isfinite(value) && value > 0.0f)
            {
                return value;
            }

            std::ostringstream oss;
            oss << kLogPrefix << label << " value " << value
                << " invalid; using fallback " << fallback << "\n";
            DebugTrace(oss.str());
            return fallback;
        };

        cellSizeWorld = sanitizePositive(cellSizeWorld, kDefaultCellSizeWorld, "cellSizeWorld");

        auto sanitizeOffsetMeters = [](float value, float fallback, const char* label)
        {
            if (std::isfinite(value))
            {
                return value;
            }

            std::ostringstream oss;
            oss << kLogPrefix << label << " invalid; using fallback " << fallback << "\n";
            DebugTrace(oss.str());
            return fallback;
        };

        osaraLeftOffsetX = sanitizeOffsetMeters(osaraLeftOffsetX, kDefaultOsaraLeftOffsetX, "osara_left_offset_x");
        osaraRightOffsetX = sanitizeOffsetMeters(osaraRightOffsetX, kDefaultOsaraRightOffsetX, "osara_right_offset_x");
        osaraOffsetY = sanitizeOffsetMeters(osaraOffsetY, kDefaultOsaraOffsetY, "osara_offset_y");
        osaraSpawnOffsetY = sanitizeOffsetMeters(osaraSpawnOffsetY, kDefaultOsaraSpawnOffsetY, "osara_spawn_offset_y");

        auto computeWorldDimension = [&](float fraction, float gridCells, float fallback)
        {
            if (std::isfinite(fraction) && fraction > 0.0f && std::isfinite(gridCells) && gridCells > 0.0f)
            {
                const float worldDimension = cellSizeWorld * gridCells * fraction;
                if (std::isfinite(worldDimension) && worldDimension > 0.0f)
                {
                    return worldDimension;
                }
            }

            return std::max(0.01f, fallback);
        };

        for (auto& descriptor : platformDescriptors)
        {
            descriptor.widthWorld = computeWorldDimension(descriptor.widthFraction, kPlatformGridWidthCells, descriptor.scale.x);
            descriptor.heightWorld = computeWorldDimension(descriptor.heightFraction, kPlatformGridHeightCells, descriptor.scale.y);
            descriptor.depthWorld = std::max(0.01f, descriptor.depth * cellSizeWorld);
        }

        if (!horizontalSpeedSet && playerMoveSpeedSet)
        {
            horizontalSpeedCellsPerSec = playerMoveSpeed / cellSizeWorld;
            horizontalSpeedSet = true;

            std::ostringstream oss;
            oss << kLogPrefix << "Derived horizontalSpeedCellsPerSec=" << horizontalSpeedCellsPerSec
                << " from legacy playerMoveSpeed=" << playerMoveSpeed
                << " and cellSizeWorld=" << cellSizeWorld << "\n";
            DebugTrace(oss.str());
        }

        horizontalSpeedCellsPerSec = sanitizePositive(
            horizontalSpeedCellsPerSec,
            kDefaultHorizontalSpeedCellsPerSec,
            "horizontalSpeedCellsPerSec");

        if (!verticalSpeedSet)
        {
            verticalSpeedCellsPerSec = horizontalSpeedCellsPerSec;
            verticalSpeedSet = true;
            std::ostringstream oss;
            oss << kLogPrefix << "verticalSpeedCellsPerSec not specified; mirroring horizontal speed ("
                << verticalSpeedCellsPerSec << ").\n";
            DebugTrace(oss.str());
        }

        verticalSpeedCellsPerSec = sanitizePositive(
            verticalSpeedCellsPerSec,
            kDefaultVerticalSpeedCellsPerSec,
            "verticalSpeedCellsPerSec");

        pickupBoxWidthCells = sanitizePositive(
            pickupBoxWidthCells,
            kDefaultPickupBoxWidthCells,
            "pickupBoxWidthCells");

        pickupBoxHeightCells = sanitizePositive(
            pickupBoxHeightCells,
            kDefaultPickupBoxHeightCells,
            "pickupBoxHeightCells");

        pickupMaxDistance = sanitizePositive(
            pickupMaxDistance,
            kDefaultPickupMaxDistance,
            "pickupMaxDistance");

        const float playerMoveSpeedWorld = horizontalSpeedCellsPerSec * cellSizeWorld;

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
        const std::size_t minirequiredMultiplierCount = minithresholds.size() + 1; //ミニフェーズ
        EnsureRewardMultiplierCoverage(rewardMultipliers, requiredMultiplierCount,
            kDefaultRewardMultipliers);
        //ミニフェーズ
        EnsureRewardMultiplierCoverage(minirewardMultipliers, minirequiredMultiplierCount,
            kDefaultMiniRewardMultipliers);

        m_planeZ = planeZ;
        m_phaseThresholds = thresholds;
        m_phaseRewardMultipliers = rewardMultipliers;
        m_phaseMiniThresholds = minithresholds;                  //ミニフェーズ
        m_phaseRewardMiniMultipliers = minirewardMultipliers;    //ミニフェーズ
        m_spawnPoints = spawnPoints;
        m_platforms = std::move(platformDescriptors);
        m_cellSizeWorld = cellSizeWorld;
        m_playerHorizontalSpeedCellsPerSec = horizontalSpeedCellsPerSec;
        m_playerVerticalSpeedCellsPerSec = verticalSpeedCellsPerSec;
        m_playerHorizontalSpeedWorld = playerMoveSpeedWorld;
        m_playerVerticalSpeedWorld = verticalSpeedCellsPerSec * cellSizeWorld;
        m_pickupBoxWidthCells = pickupBoxWidthCells;
        m_pickupBoxHeightCells = pickupBoxHeightCells;
        m_pickupBoxWidthWorld = pickupBoxWidthCells * cellSizeWorld;
        m_pickupBoxHeightWorld = pickupBoxHeightCells * cellSizeWorld;
        m_pickupMaxDistance = pickupMaxDistance;
        m_playerMoveSpeed = playerMoveSpeedWorld;
        m_guidelineHeight = guidelineHeight;
        m_heightBonusThreshold = heightBonusThreshold;
        m_heightBonusSeconds = heightBonusSeconds;
        m_backgroundTexturePath = backgroundTexturePath;
        m_osaraLeftOffsetX = osaraLeftOffsetX;
        m_osaraRightOffsetX = osaraRightOffsetX;
        m_osaraOffsetY = osaraOffsetY;
        m_osaraSpawnOffsetY = osaraSpawnOffsetY;
        m_osaraOffsetsConfigured = osaraOffsetsConfigured;
        m_spawnPointsSuppressed = spawnPointsSuppressed;
        m_sourcePath = path;
        m_loadedFromFile = !parseError;

        std::ostringstream oss;
        oss << kLogPrefix << "Loaded stage data from '" << path << "' (planeZ=" << m_planeZ
            << ", thresholds=" << m_phaseThresholds.size()
            << ", multipliers=" << m_phaseRewardMultipliers.size()
            << ", minithresholds=" << m_phaseMiniThresholds.size()            // ミニフェーズ
            << ", minimultipliers=" << m_phaseRewardMiniMultipliers.size()    // ミニフェーズ
            << ", spawnPoints=" << m_spawnPoints.size()
            << ", platforms=" << m_platforms.size()
            << ", cellSizeWorld=" << m_cellSizeWorld
            << ", playerHorizontalSpeedWorld=" << m_playerHorizontalSpeedWorld
            << ", playerVerticalSpeedWorld=" << m_playerVerticalSpeedWorld
            << ", pickupBoxWidthWorld=" << m_pickupBoxWidthWorld
            << ", pickupBoxHeightWorld=" << m_pickupBoxHeightWorld
            << ", pickupMaxDistance=" << m_pickupMaxDistance
            << ", guidelineHeight=" << m_guidelineHeight
            << ", heightBonusThreshold=" << m_heightBonusThreshold
            << ", heightBonusSeconds=" << m_heightBonusSeconds
            << ", osaraOffsetX(L/R)=(" << m_osaraLeftOffsetX << ',' << m_osaraRightOffsetX << ')'
            << ", osaraOffsetY=" << m_osaraOffsetY
            << ", osaraSpawnOffsetY=" << m_osaraSpawnOffsetY
            << ", osaraConfigured=" << std::boolalpha << m_osaraOffsetsConfigured
            << ", spawnPointsSuppressed=" << std::boolalpha << m_spawnPointsSuppressed
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
    //ミニフェーズ
    const std::vector<float>& StageData::GetPhaseMiniThresholds() const noexcept
    {
        return m_phaseMiniThresholds;
    }
    //ミニフェーズ
    const std::vector<float>& StageData::GetPhaseRewardMiniMultipliers() const noexcept
    {
        return m_phaseRewardMiniMultipliers;
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

    float StageData::GetPlayerHorizontalSpeedWorld() const noexcept
    {
        return m_playerHorizontalSpeedWorld;
    }

    float StageData::GetPlayerVerticalSpeedWorld() const noexcept
    {
        return m_playerVerticalSpeedWorld;
    }

    float StageData::GetCellSizeWorld() const noexcept
    {
        return m_cellSizeWorld;
    }

    float StageData::GetPickupBoxWidthWorld() const noexcept
    {
        return m_pickupBoxWidthWorld;
    }

    float StageData::GetPickupBoxHeightWorld() const noexcept
    {
        return m_pickupBoxHeightWorld;
    }

    float StageData::GetPickupMaxDistance() const noexcept
    {
        return m_pickupMaxDistance;
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

    float StageData::GetOsaraLeftOffsetX() const noexcept
    {
        return m_osaraLeftOffsetX;
    }

    float StageData::GetOsaraRightOffsetX() const noexcept
    {
        return m_osaraRightOffsetX;
    }

    float StageData::GetOsaraOffsetY() const noexcept
    {
        return m_osaraOffsetY;
    }

    float StageData::GetOsaraSpawnOffsetY() const noexcept
    {
        return m_osaraSpawnOffsetY;
    }

    void StageData::ApplyDefaults()
    {
        m_planeZ = kDefaultPlaneZ;
        m_phaseThresholds.assign(std::begin(kDefaultThresholds), std::end(kDefaultThresholds));
        m_phaseRewardMultipliers.assign(std::begin(kDefaultRewardMultipliers), std::end(kDefaultRewardMultipliers));
        //大フェーズ
        EnsureRewardMultiplierCoverage(m_phaseRewardMultipliers, m_phaseThresholds.size() + 1, kDefaultRewardMultipliers);
        //ミニフェーズ
        m_phaseMiniThresholds.assign(std::begin(kDefaultMiniThresholds), std::end(kDefaultMiniThresholds));
        m_phaseRewardMiniMultipliers.assign(std::begin(kDefaultMiniRewardMultipliers), std::end(kDefaultMiniRewardMultipliers));
        EnsureRewardMultiplierCoverage(m_phaseRewardMiniMultipliers, m_phaseMiniThresholds.size() + 1, kDefaultMiniRewardMultipliers);

        m_spawnPoints.assign(std::begin(kDefaultSpawnPoints), std::end(kDefaultSpawnPoints));
        m_platforms.clear();
        PlatformDescriptor descriptor{};
        descriptor.center = float3{ 0.0f, kDefaultPlatformCenterY, kDefaultPlaneZ };
        descriptor.scale = kDefaultPlatformScale;
        descriptor.depth = kDefaultPlatformDepth;
        descriptor.widthFraction = kDefaultPlatformWidthFraction;
        descriptor.heightFraction = kDefaultPlatformHeightFraction;
        descriptor.widthWorld = kDefaultPlatformWidthFraction * kPlatformGridWidthCells * kDefaultCellSizeWorld;
        descriptor.heightWorld = kDefaultPlatformHeightFraction * kPlatformGridHeightCells * kDefaultCellSizeWorld;
        descriptor.depthWorld = kDefaultPlatformDepth * kDefaultCellSizeWorld;
        descriptor.color = kDefaultPlatformPrimaryColor;
        m_platforms.push_back(descriptor);
        m_cellSizeWorld = kDefaultCellSizeWorld;
        m_playerHorizontalSpeedCellsPerSec = kDefaultHorizontalSpeedCellsPerSec;
        m_playerVerticalSpeedCellsPerSec = kDefaultVerticalSpeedCellsPerSec;
        m_playerHorizontalSpeedWorld = kDefaultHorizontalSpeedCellsPerSec * kDefaultCellSizeWorld;
        m_playerVerticalSpeedWorld = kDefaultVerticalSpeedCellsPerSec * kDefaultCellSizeWorld;
        m_pickupBoxWidthCells = kDefaultPickupBoxWidthCells;
        m_pickupBoxHeightCells = kDefaultPickupBoxHeightCells;
        m_pickupBoxWidthWorld = kDefaultPickupBoxWidthCells * kDefaultCellSizeWorld;
        m_pickupBoxHeightWorld = kDefaultPickupBoxHeightCells * kDefaultCellSizeWorld;
        m_pickupMaxDistance = kDefaultPickupMaxDistance;
        m_playerMoveSpeed = m_playerHorizontalSpeedWorld;
        m_guidelineHeight = kDefaultGuidelineHeight;
        m_backgroundTexturePath = kDefaultBackgroundTexturePath;
        m_heightBonusThreshold = kDefaultHeightBonusThreshold;
        m_heightBonusSeconds = kDefaultHeightBonusSeconds;
        m_osaraLeftOffsetX = kDefaultOsaraLeftOffsetX;
        m_osaraRightOffsetX = kDefaultOsaraRightOffsetX;
        m_osaraOffsetY = kDefaultOsaraOffsetY;
        m_osaraSpawnOffsetY = kDefaultOsaraSpawnOffsetY;
        m_osaraOffsetsConfigured = false;
        m_spawnPointsSuppressed = false;
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
            << " minithreshold=" << m_phaseMiniThresholds.size()          //ミニフェーズ
            << " minimultipliers=" << m_phaseRewardMiniMultipliers.size() //ミニフェーズ
            << " spawnPoints=" << m_spawnPoints.size()
            << " platforms=" << m_platforms.size()
            << " cellSizeWorld=" << m_cellSizeWorld
            << " playerHorizontalSpeedWorld=" << m_playerHorizontalSpeedWorld
            << " playerVerticalSpeedWorld=" << m_playerVerticalSpeedWorld
            << " pickupBoxWidthWorld=" << m_pickupBoxWidthWorld
            << " pickupBoxHeightWorld=" << m_pickupBoxHeightWorld
            << " pickupMaxDistance=" << m_pickupMaxDistance
            << " guidelineHeight=" << m_guidelineHeight
            << " heightBonusThreshold=" << m_heightBonusThreshold
            << " heightBonusSeconds=" << m_heightBonusSeconds
            << " osaraOffsetX(L/R)=(" << m_osaraLeftOffsetX << ',' << m_osaraRightOffsetX << ')'
            << " osaraOffsetY=" << m_osaraOffsetY
            << " osaraSpawnOffsetY=" << m_osaraSpawnOffsetY
            << " osaraConfigured=" << std::boolalpha << m_osaraOffsetsConfigured
            << " spawnPointsSuppressed=" << std::boolalpha << m_spawnPointsSuppressed
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
            << " worldScale=(" << descriptor.widthWorld << ',' << descriptor.heightWorld << ',' << descriptor.depthWorld << ')'
            << " widthFraction=" << descriptor.widthFraction
            << " color=(" << descriptor.color.x << ',' << descriptor.color.y << ','
            << descriptor.color.z << ',' << descriptor.color.w << ")\n";
        DebugTrace(oss.str());
    }

    void StageData::EnsureRewardMultiplierCoverage(std::vector<float>& multipliers, std::size_t requiredCount,
        const float* defaultMultipliers) const
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
