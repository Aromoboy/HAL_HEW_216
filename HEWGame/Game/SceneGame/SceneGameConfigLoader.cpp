#include "SceneGameConfigLoader.h"

#include "ConfigSectionReader.h"
#include "GameplayConfig.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <vector>

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

    constexpr const char* kLogPrefix = "[SceneGame] ";
    constexpr const char* kTechnicalConfigPath = "Assets/Config/technical.cfg";
    constexpr const char* kRenderSettingsSection = "render.settings";
    constexpr const char* kGameplayConfigPath = GameplayConfig::kConfigPath;
    constexpr float kDefaultGeneratedThicknessPadding = 1.1f;
    constexpr float3 kDefaultPlayerColliderPadding{ 0.02f, 0.05f, 0.02f };
    constexpr float3 kDefaultPlayerColliderOffset{ 0.0f, 0.0f, 0.0f };
    constexpr float3 kDefaultPlayerFallbackPadding{ 0.02f, 0.05f, 0.02f };
    constexpr float3 kDefaultPlayerFallbackOffset{ 0.0f, 0.05f, 0.0f };
    constexpr float3 kDefaultPlayerFallbackSize{ 0.6f, 0.9f, 0.12f };

    std::string TrimWhitespace(const std::string& input)
    {
        const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
        auto begin = input.begin();
        auto end = input.end();
        while (begin != end && isSpace(static_cast<unsigned char>(*begin)))
        {
            ++begin;
        }
        if (begin == end)
        {
            return std::string{};
        }
        do
        {
            --end;
        } while (end != begin && isSpace(static_cast<unsigned char>(*end)));
        return std::string(begin, end + 1);
    }

    std::optional<float> TryParseFloatToken(const std::string& token)
    {
        try
        {
            return std::stof(token);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    std::optional<float3> ParseFloat3Token(const std::string& value)
    {
        std::vector<float> components;
        std::stringstream stream(value);
        std::string token;
        while (std::getline(stream, token, ','))
        {
            token = TrimWhitespace(token);
            if (token.empty())
            {
                continue;
            }

            if (auto parsed = TryParseFloatToken(token))
            {
                components.push_back(*parsed);
            }
        }

        if (components.empty())
        {
            return std::nullopt;
        }

        if (components.size() == 1)
        {
            return float3{ components[0], components[0], components[0] };
        }

        if (components.size() == 2)
        {
            return float3{ components[0], components[1], components[1] };
        }

        return float3{ components[0], components[1], components[2] };
    }

    bool ParseBool(const std::string& value, bool& outValue)
    {
        std::string lowered = value;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });

        if (lowered == "true" || lowered == "1" || lowered == "yes" || lowered == "on")
        {
            outValue = true;
            return true;
        }

        if (lowered == "false" || lowered == "0" || lowered == "no" || lowered == "off")
        {
            outValue = false;
            return true;
        }

        return false;
    }
}

namespace SceneGameConfig
{
    PlayerColliderTuning LoadPlayerColliderTuning(const std::string& path)
    {
        PlayerColliderTuning tuning{};
        tuning.thicknessPadding = kDefaultGeneratedThicknessPadding;
        tuning.padding = kDefaultPlayerColliderPadding;
        tuning.offset = kDefaultPlayerColliderOffset;
        tuning.fallbackPadding = kDefaultPlayerFallbackPadding;
        tuning.fallbackOffset = kDefaultPlayerFallbackOffset;
        tuning.fallbackSize = kDefaultPlayerFallbackSize;

        std::vector<Config::SectionLine> lines;
        if (!Config::ReadSectionLines(path, GameplayConfig::kSectionPlayerColliderTuning, lines))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[PlayerCollider] Tuning section missing; using defaults path='" << path << "'.\n";
            DebugTrace(oss.str());
            return tuning;
        }

        for (const auto& entry : lines)
        {
            const std::string key = entry.key;
            const std::string value = entry.value;

            if (key == "thicknessPadding")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    tuning.thicknessPadding = std::max(*parsed, 1.0f);
                }
                continue;
            }

            if (key == "padding")
            {
                if (auto parsed = ParseFloat3Token(value))
                {
                    tuning.padding = *parsed;
                }
                continue;
            }

            if (key == "offset")
            {
                if (auto parsed = ParseFloat3Token(value))
                {
                    tuning.offset = *parsed;
                }
                continue;
            }

            if (key == "fallbackPadding")
            {
                if (auto parsed = ParseFloat3Token(value))
                {
                    tuning.fallbackPadding = *parsed;
                }
                continue;
            }

            if (key == "fallbackOffset")
            {
                if (auto parsed = ParseFloat3Token(value))
                {
                    tuning.fallbackOffset = *parsed;
                }
                continue;
            }

            if (key == "fallbackSize")
            {
                if (auto parsed = ParseFloat3Token(value))
                {
                    tuning.fallbackSize = *parsed;
                }
                continue;
            }
        }

        std::ostringstream oss;
        oss << kLogPrefix << "[PlayerCollider] Tuning applied pad=(" << tuning.padding.x << ',' << tuning.padding.y << ',' << tuning.padding.z
            << ") offset=(" << tuning.offset.x << ',' << tuning.offset.y << ',' << tuning.offset.z
            << ") fallbackPad=(" << tuning.fallbackPadding.x << ',' << tuning.fallbackPadding.y << ',' << tuning.fallbackPadding.z
            << ") fallbackOffset=(" << tuning.fallbackOffset.x << ',' << tuning.fallbackOffset.y << ',' << tuning.fallbackOffset.z
            << ") fallbackSize=(" << tuning.fallbackSize.x << ',' << tuning.fallbackSize.y << ',' << tuning.fallbackSize.z
            << ") thicknessPadding=" << tuning.thicknessPadding << "\n";
        DebugTrace(oss.str());

        return tuning;
    }

    bool LoadModelTextureLoadingSetting()
    {
        bool enabled = true;
        std::vector<Config::SectionLine> lines;
        if (!Config::ReadSectionLines(kTechnicalConfigPath, kRenderSettingsSection, lines))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] Missing config; defaulting loadModelTextures="
                << std::boolalpha << enabled << " section='" << kRenderSettingsSection
                << "' path='" << kTechnicalConfigPath << "'\n";
            DebugTrace(oss.str());
            return enabled;
        }

        bool parsed = false;
        for (const auto& entry : lines)
        {
            std::string key = TrimWhitespace(entry.key);
            std::string value = TrimWhitespace(entry.value);
            const std::size_t lineNumber = entry.lineNumber;
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c)
            { return static_cast<char>(std::tolower(c)); });

            if (key == "loadmodeltextures" || key == "modeltextures")
            {
                if (ParseBool(value, enabled))
                {
                    parsed = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed loadModelTextures=" << std::boolalpha << enabled
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid loadModelTextures value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << std::boolalpha << enabled << "\n";
                    DebugTrace(oss.str());
                }
            }
        }

        if (!parsed)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] loadModelTextures not found; using default="
                << std::boolalpha << enabled << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }

        return enabled;
    }

    PlayerSpawnTransform LoadPlayerTransformConfig()
    {
        PlayerSpawnTransform result{};
        constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

        auto parseFloat3 = [&](const std::string& value, float3& out, const std::string& key, int lineNumber) -> bool
        {
            std::vector<float> values;
            std::stringstream ss(value);
            std::string token;
            while (std::getline(ss, token, ','))
            {
                const std::string trimmed = TrimWhitespace(token);
                if (trimmed.empty())
                {
                    continue;
                }

                try
                {
                    values.push_back(std::stof(trimmed));
                }
                catch (const std::exception& ex)
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[PlayerTransform] Failed to parse token line=" << lineNumber
                        << " key='" << key << "' token='" << trimmed << "' reason='" << ex.what() << "'\n";
                    DebugTrace(oss.str());
                    return false;
                }
            }

            if (values.size() != 3)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerTransform] Expected 3 components for key='" << key
                    << "' on line " << lineNumber << "; got " << values.size() << "\n";
                DebugTrace(oss.str());
                return false;
            }

            out = float3{ values[0], values[1], values[2] };
            return true;
        };

        std::vector<Config::SectionLine> lines;
        if (!Config::ReadSectionLines(kGameplayConfigPath, GameplayConfig::kSectionPlayerTransform, lines))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[PlayerTransform] Config section missing; using defaults path='"
                << kGameplayConfigPath << "' section='" << GameplayConfig::kSectionPlayerTransform << "'.\n";
            DebugTrace(oss.str());
            return result;
        }

        int parsedFields = 0;
        for (const auto& entry : lines)
        {
            const int lineNumber = static_cast<int>(entry.lineNumber);
            const std::string key = TrimWhitespace(entry.key);
            const std::string value = TrimWhitespace(entry.value);
            if (key.empty() || value.empty())
            {
                continue;
            }

            float3 parsed{};
            if (key == "position" || key == "positionOffset")
            {
                if (parseFloat3(value, parsed, key, lineNumber))
                {
                    result.transform.position = parsed;
                    ++parsedFields;
                }
            }
            else if (key == "rotationDegrees" || key == "rotationDeg")
            {
                if (parseFloat3(value, parsed, key, lineNumber))
                {
                    result.transform.rotation = float3{ parsed.x * kDegToRad, parsed.y * kDegToRad, parsed.z * kDegToRad };
                    result.rotationFromDegrees = true;
                    ++parsedFields;
                }
            }
            else if (key == "rotation" || key == "rotationRadians")
            {
                if (parseFloat3(value, parsed, key, lineNumber))
                {
                    result.transform.rotation = parsed;
                    result.rotationFromDegrees = false;
                    ++parsedFields;
                }
            }
            else if (key == "scale")
            {
                if (parseFloat3(value, parsed, key, lineNumber))
                {
                    result.transform.scale = parsed;
                    ++parsedFields;
                }
            }
            else
            {
                std::ostringstream oss;
                oss << kLogPrefix << "[PlayerTransform] Ignored unknown key '" << key << "' on line " << lineNumber << "\n";
                DebugTrace(oss.str());
            }
        }

        result.loadedFromFile = true;

        std::ostringstream oss;
        oss << kLogPrefix << "[PlayerTransform] Applied path='" << kGameplayConfigPath
            << "' section='" << GameplayConfig::kSectionPlayerTransform
            << "' loaded=" << std::boolalpha << result.loadedFromFile
            << " parsedFields=" << parsedFields
            << " position=(" << result.transform.position.x << ',' << result.transform.position.y << ',' << result.transform.position.z << ')'
            << " rotationRad=(" << result.transform.rotation.x << ',' << result.transform.rotation.y << ',' << result.transform.rotation.z << ')'
            << " scale=(" << result.transform.scale.x << ',' << result.transform.scale.y << ',' << result.transform.scale.z << ')'
            << " rotationSource=" << (result.rotationFromDegrees ? "degrees" : "radians_or_default") << "\n";
        DebugTrace(oss.str());

        return result;
    }

    PhysicsTuningSettings LoadPhysicsTuningSettings()
    {
        PhysicsTuningSettings settings;
        std::vector<Config::SectionLine> lines;
        if (!Config::ReadSectionLines(kGameplayConfigPath, GameplayConfig::kSectionPhysicsTuning, lines))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Physics tuning config section '" << GameplayConfig::kSectionPhysicsTuning
                << "' not found at '" << kGameplayConfigPath
                << "'. Using defaults (gravity=" << settings.gravity
                << ", subSteps=" << settings.subStepCount
                << ", linearDamping=" << settings.linearDamping
                << ", angularDamping=" << settings.angularDamping
                << ", defaultFriction=" << settings.defaultFriction
                << ", defaultRestitution=" << settings.defaultRestitution
                << ", restitutionThreshold=" << settings.restitutionThreshold
                << ", hitEventThreshold=" << settings.hitEventThreshold
                << ", maxStepDt=" << settings.maxStepDt
                << ", maxAccumulatedSteps=" << settings.maxAccumulatedSteps
                << ", ccdSweets=" << std::boolalpha << settings.continuousCollisionForSweets
                << ", ccdSmallDynamics=" << settings.continuousCollisionForSmallDynamics
                << ", ccdSmallDynamicThreshold=" << settings.smallDynamicContinuousThreshold
                << ", ccdPlayerProjectiles=" << settings.continuousCollisionForPlayerProjectiles
                << ", ccdPlayer=" << settings.continuousCollisionForPlayer
                << ").\n";
            ::DebugTrace(oss.str());
            return settings;
        }

        auto parseBool = [](const std::string& value) -> std::optional<bool>
        {
            std::string lower = value;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });

            if (lower == "1" || lower == "true")
            {
                return true;
            }

            if (lower == "0" || lower == "false")
            {
                return false;
            }

            return std::nullopt;
        };

        for (const auto& entry : lines)
        {
            const std::size_t lineNumber = entry.lineNumber;
            std::string key = TrimWhitespace(entry.key);
            std::string value = TrimWhitespace(entry.value);
            if (key.empty() || value.empty())
            {
                continue;
            }

            try
            {
                if (key == "gravity")
                {
                    settings.gravity = std::stof(value);
                }
                else if (key == "subSteps")
                {
                    settings.subStepCount = std::max(1, std::stoi(value));
                }
                else if (key == "linearDamping")
                {
                    settings.linearDamping = std::max(0.0f, std::stof(value));
                }
                else if (key == "angularDamping")
                {
                    settings.angularDamping = std::max(0.0f, std::stof(value));
                }
                else if (key == "defaultFriction")
                {
                    settings.defaultFriction = std::max(0.0f, std::stof(value));
                }
                else if (key == "defaultRestitution")
                {
                    settings.defaultRestitution = std::clamp(std::stof(value), 0.0f, 1.0f);
                }
                else if (key == "restitutionThreshold")
                {
                    settings.restitutionThreshold = std::max(0.0f, std::stof(value));
                }
                else if (key == "hitEventThreshold")
                {
                    settings.hitEventThreshold = std::max(0.0f, std::stof(value));
                }
                else if (key == "maxStepDt")
                {
                    settings.maxStepDt = std::max(0.0f, std::stof(value));
                }
                else if (key == "maxAccumulatedSteps")
                {
                    settings.maxAccumulatedSteps = std::max(1, std::stoi(value));
                }
                else if (key == "ccdSweets")
                {
                    if (auto parsed = parseBool(value))
                    {
                        settings.continuousCollisionForSweets = *parsed;
                    }
                    else
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Invalid bool for key '" << key << "' on line "
                            << lineNumber << "\n";
                        ::DebugTrace(oss.str());
                    }
                }
                else if (key == "ccdSmallDynamics")
                {
                    if (auto parsed = parseBool(value))
                    {
                        settings.continuousCollisionForSmallDynamics = *parsed;
                    }
                    else
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Invalid bool for key '" << key << "' on line "
                            << lineNumber << "\n";
                        ::DebugTrace(oss.str());
                    }
                }
                else if (key == "ccdSmallDynamicThreshold")
                {
                    settings.smallDynamicContinuousThreshold = std::max(0.0f, std::stof(value));
                }
                else if (key == "ccdPlayerProjectiles")
                {
                    if (auto parsed = parseBool(value))
                    {
                        settings.continuousCollisionForPlayerProjectiles = *parsed;
                    }
                    else
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Invalid bool for key '" << key << "' on line "
                            << lineNumber << "\n";
                        ::DebugTrace(oss.str());
                    }
                }
                else if (key == "ccdPlayer")
                {
                    if (auto parsed = parseBool(value))
                    {
                        settings.continuousCollisionForPlayer = *parsed;
                    }
                    else
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Invalid bool for key '" << key << "' on line "
                            << lineNumber << "\n";
                        ::DebugTrace(oss.str());
                    }
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Unknown physics tuning key '" << key << "' on line "
                        << lineNumber << "\n";
                    ::DebugTrace(oss.str());
                }
            }
            catch (const std::exception& ex)
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Failed to parse value for key '" << key << "' on line "
                    << lineNumber << ": " << ex.what() << "\n";
                ::DebugTrace(oss.str());
            }
        }

        std::ostringstream oss;
        oss << kLogPrefix << "Loaded physics tuning from '" << kGameplayConfigPath
            << "' section='" << GameplayConfig::kSectionPhysicsTuning
            << "' (gravity=" << settings.gravity
            << ", subSteps=" << settings.subStepCount
            << ", linearDamping=" << settings.linearDamping
            << ", angularDamping=" << settings.angularDamping
            << ", defaultFriction=" << settings.defaultFriction
            << ", defaultRestitution=" << settings.defaultRestitution
            << ", restitutionThreshold=" << settings.restitutionThreshold
            << ", hitEventThreshold=" << settings.hitEventThreshold
            << ", maxStepDt=" << settings.maxStepDt
            << ", maxAccumulatedSteps=" << settings.maxAccumulatedSteps
            << ", ccdSweets=" << std::boolalpha << settings.continuousCollisionForSweets
            << ", ccdSmallDynamics=" << settings.continuousCollisionForSmallDynamics
            << ", ccdSmallDynamicThreshold=" << settings.smallDynamicContinuousThreshold
            << ", ccdPlayerProjectiles=" << settings.continuousCollisionForPlayerProjectiles
            << ", ccdPlayer=" << settings.continuousCollisionForPlayer
            << ").\n";
        ::DebugTrace(oss.str());
        return settings;
    }
}
