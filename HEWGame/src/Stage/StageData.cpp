// ============================================================================
// 概要: StageData の挙動や関連ロジックを実装し、src／Stage に属する機能を具体化している。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: src／Stage 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

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
    const float3 kDefaultPlatformScale{ 12.0f, 2.0f, 4.0f };
    constexpr float kDefaultPlayerMoveSpeed = 15.0f;

    const float3 kDefaultSpawnPoints[] =
    {
        { -2.5f, 0.0f, kDefaultPlaneZ },
        { 0.0f, 0.0f, kDefaultPlaneZ },
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
}

namespace Stage
{
    StageData::StageData()
        : m_planeZ(kDefaultPlaneZ)
        , m_platformCenterY(kDefaultPlatformCenterY)
        , m_platformScale(kDefaultPlatformScale)
        , m_loadedFromFile(false)
        , m_playerMoveSpeed(kDefaultPlayerMoveSpeed)
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
        float3 platformScale = kDefaultPlatformScale;
        bool platformScaleSet = false;
        float playerMoveSpeed = kDefaultPlayerMoveSpeed;
        bool playerMoveSpeedSet = false;
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
                bool error = false;
                auto parsed = ParseFloatList(rawValue, &error);
                if (error || parsed.size() < 3)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "spawnPoint on line " << lineNumber
                        << " must contain three floats. Raw='" << rawValue << "'\n";
                    DebugTrace(oss.str());
                    parseError = true;
                }
                else
                {
                    spawnPoints.push_back(float3{ parsed[0], parsed[1], parsed[2] });
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

        const std::size_t requiredMultiplierCount = thresholds.size() + 1;
        EnsureRewardMultiplierCoverage(rewardMultipliers, requiredMultiplierCount);

        m_planeZ = planeZ;
        m_phaseThresholds = thresholds;
        m_phaseRewardMultipliers = rewardMultipliers;
        m_spawnPoints = spawnPoints;
        m_platformCenterY = platformCenterY;
        m_platformScale = platformScale;
        m_playerMoveSpeed = playerMoveSpeed;
        m_sourcePath = path;
        m_loadedFromFile = !parseError;

        std::ostringstream oss;
        oss << kLogPrefix << "Loaded stage data from '" << path << "' (planeZ=" << m_planeZ
            << ", thresholds=" << m_phaseThresholds.size()
            << ", multipliers=" << m_phaseRewardMultipliers.size()
            << ", spawnPoints=" << m_spawnPoints.size()
            << ", platformCenterY=" << m_platformCenterY
            << ", platformScale=(" << m_platformScale.x << ',' << m_platformScale.y << ',' << m_platformScale.z << ')'
            << ", playerMoveSpeed=" << m_playerMoveSpeed
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

    void StageData::ApplyDefaults()
    {
        m_planeZ = kDefaultPlaneZ;
        m_platformCenterY = kDefaultPlatformCenterY;
        m_platformScale = kDefaultPlatformScale;
        m_phaseThresholds.assign(std::begin(kDefaultThresholds), std::end(kDefaultThresholds));
        m_phaseRewardMultipliers.assign(std::begin(kDefaultRewardMultipliers), std::end(kDefaultRewardMultipliers));
        EnsureRewardMultiplierCoverage(m_phaseRewardMultipliers, m_phaseThresholds.size() + 1);
        m_spawnPoints.assign(std::begin(kDefaultSpawnPoints), std::end(kDefaultSpawnPoints));
        m_playerMoveSpeed = kDefaultPlayerMoveSpeed;
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
            << " platformScale=(" << m_platformScale.x << ',' << m_platformScale.y << ',' << m_platformScale.z << ')'
            << " playerMoveSpeed=" << m_playerMoveSpeed
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
