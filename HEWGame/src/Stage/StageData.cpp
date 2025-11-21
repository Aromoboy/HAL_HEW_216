#include "Stage/StageData.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
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
}

namespace Stage
{
    StageData::StageData()
        : m_planeZ(kDefaultPlaneZ)
        , m_platformCenterY(kDefaultPlatformCenterY)
        , m_platformWidthFraction(kDefaultPlatformWidthFraction)
        , m_platformHeightFraction(kDefaultPlatformHeightFraction)
        , m_platformDepth(kDefaultPlatformDepth)
        , m_platformScale(kDefaultPlatformScale)
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

            if (key == "planeZ")
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

        if (!std::isfinite(platformDepth) || platformDepth == 0.0f)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "platformDepth value " << platformDepth
                << " invalid; using default " << kDefaultPlatformDepth << "\n";
            DebugTrace(oss.str());
            platformDepth = kDefaultPlatformDepth;
        }
        platformDepth = std::max(0.01f, std::fabs(platformDepth));

        auto sanitizeScale = [](float value)
        {
            return std::max(0.01f, std::fabs(value));
        };
        const float3 rawScale = platformScale;
        platformScale.x = sanitizeScale(platformScale.x);
        platformScale.y = sanitizeScale(platformScale.y);
        platformScale.z = sanitizeScale(platformScale.z);
        if (platformScale.x != rawScale.x || platformScale.y != rawScale.y || platformScale.z != rawScale.z)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "platformScale values sanitized from ("
                << rawScale.x << ',' << rawScale.y << ',' << rawScale.z << ") to ("
                << platformScale.x << ',' << platformScale.y << ',' << platformScale.z << ").\n";
            DebugTrace(oss.str());
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
        m_platformCenterY = platformCenterY;
        m_platformWidthFraction = platformWidthFraction;
        m_platformHeightFraction = platformHeightFraction;
        m_platformDepth = platformDepth;
        m_platformScale = platformScale;
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
            << ", platformCenterY=" << m_platformCenterY
            << ", platformWidthFraction=" << m_platformWidthFraction
            << ", platformHeightFraction=" << m_platformHeightFraction
            << ", platformDepth=" << m_platformDepth
            << ", platformScale=(" << m_platformScale.x << ',' << m_platformScale.y << ',' << m_platformScale.z << ')'
            << ", playerMoveSpeed=" << m_playerMoveSpeed
            << ", guidelineHeight=" << m_guidelineHeight
            << ", heightBonusThreshold=" << m_heightBonusThreshold
            << ", heightBonusSeconds=" << m_heightBonusSeconds
            << ", backgroundTexture='" << m_backgroundTexturePath << "'"
            << ", parseError=" << std::boolalpha << parseError << ").\n";
        DebugTrace(oss.str());

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

    float StageData::GetPlatformCenterY() const noexcept
    {
        return m_platformCenterY;
    }

    float StageData::GetPlatformWidthFraction() const noexcept
    {
        return m_platformWidthFraction;
    }

    float StageData::GetPlatformHeightFraction() const noexcept
    {
        return m_platformHeightFraction;
    }

    float StageData::GetPlatformDepth() const noexcept
    {
        return m_platformDepth;
    }

    const float3& StageData::GetPlatformScale() const noexcept
    {
        return m_platformScale;
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
        m_platformCenterY = kDefaultPlatformCenterY;
        m_platformWidthFraction = kDefaultPlatformWidthFraction;
        m_platformHeightFraction = kDefaultPlatformHeightFraction;
        m_platformDepth = kDefaultPlatformDepth;
        m_platformScale = kDefaultPlatformScale;
        m_phaseThresholds.assign(std::begin(kDefaultThresholds), std::end(kDefaultThresholds));
        m_phaseRewardMultipliers.assign(std::begin(kDefaultRewardMultipliers), std::end(kDefaultRewardMultipliers));
        EnsureRewardMultiplierCoverage(m_phaseRewardMultipliers, m_phaseThresholds.size() + 1);
        m_spawnPoints.assign(std::begin(kDefaultSpawnPoints), std::end(kDefaultSpawnPoints));
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
            << " platformCenterY=" << m_platformCenterY
            << " platformWidthFraction=" << m_platformWidthFraction
            << " platformHeightFraction=" << m_platformHeightFraction
            << " platformDepth=" << m_platformDepth
            << " platformScale=(" << m_platformScale.x << ',' << m_platformScale.y << ',' << m_platformScale.z << ')'
            << " playerMoveSpeed=" << m_playerMoveSpeed
            << " guidelineHeight=" << m_guidelineHeight
            << " heightBonusThreshold=" << m_heightBonusThreshold
            << " heightBonusSeconds=" << m_heightBonusSeconds
            << " backgroundTexture='" << m_backgroundTexturePath << "'"
            << " detail=" << detail << "\n";
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
