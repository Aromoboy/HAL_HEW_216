#include "Stage/StageConfigParser.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <stdexcept>

#include "Stage/StageConfigNormalize.h"

namespace
{
    constexpr const char* kLogPrefix = "[StageData] ";
    constexpr std::size_t kSpawnCoordinatesPerAnchor = 3;

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
                return {};
            }
        }
        if (values.empty() && outError)
        {
            *outError = true;
        }
        return values;
    }

    std::string ToLowerCopy(const std::string& value)
    {
        std::string lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
        return lowered;
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
            return false;
        }

        if (parsed.size() % kSpawnCoordinatesPerAnchor != 0)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Spawn point on line " << lineNumber
                << " expected groups of three floats but found " << parsed.size()
                << " values. Raw='" << rawValue << "'\n";
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
}

namespace Stage
{
    bool TryParsePickupOffsetMode(const std::string& rawValue, Stage::PickupOffsetMode& outMode)
    {
        const std::string lowered = ToLowerCopy(rawValue);
        if (lowered == "none")
        {
            outMode = Stage::PickupOffsetMode::None;
            return true;
        }
        if (lowered == "facing" || lowered == "facingdirection" || lowered == "facing-direction")
        {
            outMode = Stage::PickupOffsetMode::FacingDirection;
            return true;
        }
        if (lowered == "worldx" || lowered == "world-x" || lowered == "world_x")
        {
            outMode = Stage::PickupOffsetMode::WorldX;
            return true;
        }

        return false;
    }

    const char* PickupOffsetModeToString(Stage::PickupOffsetMode mode)
    {
        switch (mode)
        {
        case Stage::PickupOffsetMode::None: return "none";
        case Stage::PickupOffsetMode::FacingDirection: return "facing-direction";
        case Stage::PickupOffsetMode::WorldX: return "world-x";
        default: return "unknown";
        }
    }

    StageConfigParseResult ParseStageConfig(const std::vector<Config::SectionLine>& lines)
    {
        StageConfigParseResult result{};
        result.planeZ = StageConfigDefaults::kDefaultPlaneZ;
        result.platformCenterY = StageConfigDefaults::kDefaultPlatformCenterY;
        result.platformWidthFraction = StageConfigDefaults::kDefaultPlatformWidthFraction;
        result.platformHeightFraction = StageConfigDefaults::kDefaultPlatformHeightFraction;
        result.platformDepth = StageConfigDefaults::kDefaultPlatformDepth;
        result.platformScale = StageConfigDefaults::DefaultPlatformScale();
        result.cellSizeWorld = StageConfigDefaults::kDefaultCellSizeWorld;
        result.horizontalSpeedCellsPerSec = StageConfigDefaults::kDefaultHorizontalSpeedCellsPerSec;
        result.verticalSpeedCellsPerSec = StageConfigDefaults::kDefaultVerticalSpeedCellsPerSec;
        result.pickupBoxWidthCells = StageConfigDefaults::kDefaultPickupBoxWidthCells;
        result.pickupBoxHeightCells = StageConfigDefaults::kDefaultPickupBoxHeightCells;
        result.pickupMaxDistance = StageConfigDefaults::kDefaultPickupMaxDistance;
        result.pickupDepthThreshold = StageConfigDefaults::kDefaultPickupDepthThreshold;
        result.pickupOffsetMode = StageConfigDefaults::kDefaultPickupOffsetMode;
        result.pickupOffsetLocalCells = StageConfigDefaults::DefaultPickupOffsetLocalCells();
        result.guidelineHeight = StageConfigDefaults::kDefaultGuidelineHeight;
        result.guidelineStep = StageConfigDefaults::kDefaultGuidelineStep;
        result.guidelineThicknessPixels = StageConfigDefaults::kDefaultGuidelineThicknessPixels;
        result.guidelineWidthPaddingWorld = StageConfigDefaults::DefaultGuidelineWidthPaddingWorld();
        result.guidelineZOffset = StageConfigDefaults::kDefaultGuidelineZOffset;
        result.guidelineModelPath = StageConfigDefaults::DefaultGuidelineModelPath();
        result.guidelineMaterialKey = StageConfigDefaults::DefaultGuidelineMaterialKey();
        result.heightBonusThreshold = StageConfigDefaults::kDefaultHeightBonusThreshold;
        result.heightBonusSeconds = StageConfigDefaults::kDefaultHeightBonusSeconds;
        result.backgroundTexturePath = StageConfigDefaults::DefaultBackgroundTexturePath();

        for (const auto& entry : lines)
        {
            const std::string key = entry.key;
            const std::string rawValue = entry.value;
            const std::size_t lineNumber = entry.lineNumber;
            auto appendSpawnPoints = [&](const std::string& label, const std::string& spawnValue, std::vector<float3>& targets)
            {
                if (label == "spawnPointsPrimary")
                {
                    result.primaryProvided = true;
                }
                else if (label == "spawnPointsSecondary")
                {
                    result.secondaryProvided = true;
                }

                const size_t beforeCount = targets.size();
                if (!TryParseSpawnPoint(spawnValue, lineNumber, targets))
                {
                    result.parseError = true;
                    if (label == "spawnPointsPrimary")
                    {
                        result.primaryParseError = true;
                    }
                    else if (label == "spawnPointsSecondary")
                    {
                        result.secondaryParseError = true;
                    }
                    return;
                }

                const size_t added = targets.size() - beforeCount;
                std::ostringstream oss;
                oss << kLogPrefix << label << " on line " << lineNumber
                    << " parsed " << added << " anchor" << (added == 1 ? "" : "s")
                    << " (" << targets.size() << " total). Each anchor requires"
                    << " three coordinates. Raw='" << spawnValue << "'\n";
            };

            size_t platformIndex = 0;
            if (TryParsePlatformKey(key, "center", platformIndex))
            {
                auto ensureOverride = [&](size_t index)
                {
                    if (index >= result.platformOverrides.size())
                    {
                        result.platformOverrides.resize(index + 1);
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
                    result.parseError = true;
                }
                else
                {
                    result.platformOverrides[platformIndex].descriptor.center = float3{ parsed[0], parsed[1], parsed[2] };
                    result.platformOverrides[platformIndex].centerProvided = true;
                }
            }
            else if (TryParsePlatformKey(key, "scale", platformIndex))
            {
                auto ensureOverride = [&](size_t index)
                {
                    if (index >= result.platformOverrides.size())
                    {
                        result.platformOverrides.resize(index + 1);
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
                    result.parseError = true;
                }
                else
                {
                    result.platformOverrides[platformIndex].descriptor.scale = float3{ parsed[0], parsed[1], parsed[2] };
                    result.platformOverrides[platformIndex].scaleProvided = true;
                }
            }
            else if (TryParsePlatformKey(key, "depth", platformIndex))
            {
                auto ensureOverride = [&](size_t index)
                {
                    if (index >= result.platformOverrides.size())
                    {
                        result.platformOverrides.resize(index + 1);
                    }
                };
                ensureOverride(platformIndex);

                try
                {
                    result.platformOverrides[platformIndex].descriptor.depth = std::stof(rawValue);
                    result.platformOverrides[platformIndex].depthProvided = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse platform" << platformIndex
                        << ".depth on line " << lineNumber << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (TryParsePlatformKey(key, "widthFraction", platformIndex))
            {
                auto ensureOverride = [&](size_t index)
                {
                    if (index >= result.platformOverrides.size())
                    {
                        result.platformOverrides.resize(index + 1);
                    }
                };
                ensureOverride(platformIndex);

                try
                {
                    result.platformOverrides[platformIndex].descriptor.widthFraction = std::stof(rawValue);
                    result.platformOverrides[platformIndex].widthProvided = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse platform" << platformIndex
                        << ".widthFraction on line " << lineNumber << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (TryParsePlatformKey(key, "heightFraction", platformIndex))
            {
                auto ensureOverride = [&](size_t index)
                {
                    if (index >= result.platformOverrides.size())
                    {
                        result.platformOverrides.resize(index + 1);
                    }
                };
                ensureOverride(platformIndex);

                try
                {
                    result.platformOverrides[platformIndex].descriptor.heightFraction = std::stof(rawValue);
                    result.platformOverrides[platformIndex].heightProvided = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse platform" << platformIndex
                        << ".heightFraction on line " << lineNumber << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (TryParsePlatformKey(key, "color", platformIndex))
            {
                auto ensureOverride = [&](size_t index)
                {
                    if (index >= result.platformOverrides.size())
                    {
                        result.platformOverrides.resize(index + 1);
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
                    result.parseError = true;
                }
                else
                {
                    DirectX::XMFLOAT4 color{ parsed[0], parsed[1], parsed[2], 1.0f };
                    if (parsed.size() >= 4)
                    {
                        color.w = parsed[3];
                    }
                    result.platformOverrides[platformIndex].descriptor.color = color;
                    result.platformOverrides[platformIndex].colorProvided = true;
                }
            }
            else if (key == "planeZ")
            {
                try
                {
                    result.planeZ = std::stof(rawValue);
                    result.planeZSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse planeZ on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "phaseThresholds")
            {
                bool error = false;
                auto parsed = ParseFloatList(rawValue, &error);
                if (error)
                {
                    result.parseError = true;
                }
                else
                {
                    result.thresholds.insert(result.thresholds.end(), parsed.begin(), parsed.end());
                }
            }
            //ミニフェーズ
            else if (key == "phaseMiniThresholds")
            {
                bool error = false;
                auto parsed = ParseFloatList(rawValue, &error);
                if (error)
                {
                    result.parseError = true;
                }
                else
                {
                    result.minithresholds.insert(result.minithresholds.end(), parsed.begin(), parsed.end());
                }
            }
            else if (key == "spawnPoints" || key == "spawnPointsPrimary" || key == "spawnPointsA")
            {
                appendSpawnPoints("spawnPointsPrimary", rawValue, result.spawnPointsPrimary);
            }
            else if (key == "spawnPointsSecondary" || key == "spawnPointsB")
            {
                appendSpawnPoints("spawnPointsSecondary", rawValue, result.spawnPointsSecondary);
            }
            else if (key == "phaseRewardMultipliers")
            {
                bool error = false;
                auto parsed = ParseFloatList(rawValue, &error);
                if (error)
                {
                    result.parseError = true;
                }
                else
                {
                    result.rewardMultipliers.insert(result.rewardMultipliers.end(), parsed.begin(), parsed.end());
                }
            }
            //ミニフェーズ
            else if (key == "phaseMiniRewardMultipliers")
            {
                bool error = false;
                auto parsed = ParseFloatList(rawValue, &error);
                if (error)
                {
                    result.parseError = true;
                }
                else
                {
                    result.minirewardMultipliers.insert(result.minirewardMultipliers.end(), parsed.begin(), parsed.end());
                }
            }
            else if (key == "platformCenterY")
            {
                try
                {
                    result.platformCenterY = std::stof(rawValue);
                    result.platformCenterSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse platformCenterY on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "platformWidthFraction")
            {
                try
                {
                    result.platformWidthFraction = std::stof(rawValue);
                    result.platformWidthFractionSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse platformWidthFraction on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "platformHeightFraction")
            {
                try
                {
                    result.platformHeightFraction = std::stof(rawValue);
                    result.platformHeightFractionSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse platformHeightFraction on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "platformDepth")
            {
                try
                {
                    result.platformDepth = std::stof(rawValue);
                    result.platformDepthSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse platformDepth on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
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
                    result.parseError = true;
                }
                else
                {
                    result.platformScale = float3{ parsed[0], parsed[1], parsed[2] };
                    result.platformScaleSet = true;
                }
            }
            else if (key == "cellSizeWorld")
            {
                try
                {
                    result.cellSizeWorld = std::stof(rawValue);
                    result.cellSizeWorldSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse cellSizeWorld on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "horizontalSpeedCellsPerSec")
            {
                try
                {
                    result.horizontalSpeedCellsPerSec = std::stof(rawValue);
                    result.horizontalSpeedSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse horizontalSpeedCellsPerSec on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "verticalSpeedCellsPerSec")
            {
                try
                {
                    result.verticalSpeedCellsPerSec = std::stof(rawValue);
                    result.verticalSpeedSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse verticalSpeedCellsPerSec on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "pickupBoxWidthCells")
            {
                try
                {
                    result.pickupBoxWidthCells = std::stof(rawValue);
                    result.pickupBoxWidthSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse pickupBoxWidthCells on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "pickupBoxHeightCells")
            {
                try
                {
                    result.pickupBoxHeightCells = std::stof(rawValue);
                    result.pickupBoxHeightSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse pickupBoxHeightCells on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "pickupMaxDistance")
            {
                try
                {
                    result.pickupMaxDistance = std::stof(rawValue);
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse pickupMaxDistance on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "pickupDepthThreshold")
            {
                try
                {
                    result.pickupDepthThreshold = std::stof(rawValue);
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse pickupDepthThreshold on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "pickupOffsetMode")
            {
                Stage::PickupOffsetMode parsedMode{};
                if (TryParsePickupOffsetMode(rawValue, parsedMode))
                {
                    result.pickupOffsetMode = parsedMode;
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "pickupOffsetMode on line " << lineNumber
                        << " is invalid ('" << rawValue << "'); expected none/facing-direction/world-x.\n";
                    result.parseError = true;
                }
            }
            else if (key == "pickupOffsetLocalCells")
            {
                auto parsed = ParseFloatList(rawValue, nullptr);
                if (parsed.size() == 3)
                {
                    result.pickupOffsetLocalCells = float3{ parsed[0], parsed[1], parsed[2] };
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "pickupOffsetLocalCells on line " << lineNumber
                        << " must contain three floats. Raw='" << rawValue << "'\n";
                    result.parseError = true;
                }
            }
            else if (key == "guidelineHeight")
            {
                try
                {
                    result.guidelineHeight = std::stof(rawValue);
                    result.guidelineHeightSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse guidelineHeight on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "guidelineStep")
            {
                try
                {
                    result.guidelineStep = std::stof(rawValue);
                    result.guidelineStepSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse guidelineStep on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "guidelineThicknessPixels")
            {
                try
                {
                    result.guidelineThicknessPixels = std::stof(rawValue);
                    result.guidelineThicknessSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse guidelineThicknessPixels on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "guidelineWidthPaddingWorld")
            {
                try
                {
                    result.guidelineWidthPaddingWorld = std::stof(rawValue);
                    result.guidelineWidthPaddingSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse guidelineWidthPaddingWorld on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "guidelineZOffset")
            {
                try
                {
                    result.guidelineZOffset = std::stof(rawValue);
                    result.guidelineZOffsetSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse guidelineZOffset on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "guidelineModelPath")
            {
                if (rawValue.empty())
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "guidelineModelPath on line " << lineNumber
                        << " was empty; fallback guideline rendering will be used.\n";
                    result.guidelineModelPath.clear();
                    result.guidelineModelPathSet = true;
                }
                else
                {
                    result.guidelineModelPath = rawValue;
                    result.guidelineModelPathSet = true;
                }
            }
            else if (key == "guidelineMaterialKey")
            {
                if (rawValue.empty())
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "guidelineMaterialKey on line " << lineNumber
                        << " was empty; default material key will be used.\n";
                    result.guidelineMaterialKey.clear();
                    result.guidelineMaterialKeySet = true;
                }
                else
                {
                    result.guidelineMaterialKey = rawValue;
                    result.guidelineMaterialKeySet = true;
                }
            }
            else if (key == "heightBonusThreshold")
            {
                try
                {
                    result.heightBonusThreshold = std::stof(rawValue);
                    result.heightBonusThresholdSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse heightBonusThreshold on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "heightBonusSeconds")
            {
                try
                {
                    result.heightBonusSeconds = std::stof(rawValue);
                    result.heightBonusSecondsSet = true;
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Failed to parse heightBonusSeconds on line " << lineNumber
                        << ": " << ex.what() << "\n";
                    result.parseError = true;
                }
            }
            else if (key == "backgroundTexture")
            {
                if (rawValue.empty())
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "backgroundTexture on line " << lineNumber
                        << " was empty; background rendering disabled.\n";
                    result.backgroundTexturePath.clear();
                    result.backgroundTextureSet = true;
                }
                else
                {
                    result.backgroundTexturePath = rawValue;
                    result.backgroundTextureSet = true;
                }
            }
            else
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Unknown key '" << key << "' on line " << lineNumber << "\n";
            }
        }

        return result;
    }
}
