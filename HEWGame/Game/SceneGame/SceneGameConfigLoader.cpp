#include "SceneGameConfigLoader.h"

#include "ConfigSectionReader.h"
#include "GameplayConfig.h"
#include "MaterialTuningSettings.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>
#include <DirectXMath.h>

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
    constexpr const char* kRenderFadeSection = "render.fade";
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

    std::string ToLowerCopy(const std::string& input)
    {
        std::string lowered = input;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
        return lowered;
    }

    bool StartsWith(const std::string& value, const std::string& prefix)
    {
        return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
    }

    bool EndsWith(const std::string& value, const std::string& suffix)
    {
        return value.size() >= suffix.size() && value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    std::vector<std::string> ParseTagList(const std::string& value)
    {
        std::vector<std::string> tags;
        std::stringstream stream(value);
        std::string token;
        while (std::getline(stream, token, ','))
        {
            std::string trimmed = TrimWhitespace(token);
            if (trimmed.empty())
            {
                continue;
            }
            trimmed = ToLowerCopy(trimmed);
            tags.push_back(std::move(trimmed));
        }
        return tags;
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

    std::optional<int> TryParseIntToken(const std::string& token)
    {
        try
        {
            size_t pos = 0;
            int value = std::stoi(token, &pos);
            if (pos != token.size())
            {
                return std::nullopt;
            }
            return value;
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

    std::optional<DirectX::XMFLOAT4> ParseFloat4Token(const std::string& value, float defaultW)
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
            return DirectX::XMFLOAT4{ components[0], components[0], components[0], defaultW };
        }

        if (components.size() == 2)
        {
            return DirectX::XMFLOAT4{ components[0], components[1], components[1], defaultW };
        }

        if (components.size() == 3)
        {
            return DirectX::XMFLOAT4{ components[0], components[1], components[2], defaultW };
        }

        return DirectX::XMFLOAT4{ components[0], components[1], components[2], components[3] };
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

    std::optional<SceneGameConfig::CameraRigMode> ParseCameraRigMode(const std::string& value)
    {
        const std::string lowered = ToLowerCopy(TrimWhitespace(value));
        if (lowered == "fixed" || lowered == "static")
        {
            return SceneGameConfig::CameraRigMode::Fixed;
        }
        if (lowered == "follow" || lowered == "tracking")
        {
            return SceneGameConfig::CameraRigMode::Follow;
        }
        if (lowered == "cinematic" || lowered == "drama")
        {
            return SceneGameConfig::CameraRigMode::Cinematic;
        }
        return std::nullopt;
    }

    std::optional<SceneGameConfig::RenderFadeEasing> ParseRenderFadeEasing(const std::string& value)
    {
        const std::string lowered = ToLowerCopy(TrimWhitespace(value));
        if (lowered == "linear")
        {
            return SceneGameConfig::RenderFadeEasing::Linear;
        }
        if (lowered == "smoothstep" || lowered == "smooth")
        {
            return SceneGameConfig::RenderFadeEasing::SmoothStep;
        }
        if (lowered == "easein" || lowered == "in")
        {
            return SceneGameConfig::RenderFadeEasing::EaseIn;
        }
        if (lowered == "easeout" || lowered == "out")
        {
            return SceneGameConfig::RenderFadeEasing::EaseOut;
        }
        if (lowered == "easeinout" || lowered == "inout" || lowered == "ease")
        {
            return SceneGameConfig::RenderFadeEasing::EaseInOut;
        }
        return std::nullopt;
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

    RenderPostProcessSettings LoadRenderPostProcessSettings()
    {
        RenderPostProcessSettings settings{};
        std::vector<Config::SectionLine> lines;
        if (!Config::ReadSectionLines(kTechnicalConfigPath, kRenderSettingsSection, lines))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] Missing config; defaulting post-process settings section='"
                << kRenderSettingsSection << "' path='" << kTechnicalConfigPath << "'\n";
            DebugTrace(oss.str());
            return settings;
        }

        bool parsedGamma = false;
        bool parsedSaturation = false;
        bool parsedContrast = false;
        bool parsedFogColor = false;
        bool parsedFogStart = false;
        bool parsedFogRange = false;
        bool parsedVignetteStrength = false;
        bool parsedBloomThreshold = false;
        bool parsedBloomIntensity = false;
        bool parsedGrainAmount = false;
        bool parsedFxaaEdgeThreshold = false;
        bool parsedFxaaEdgeThresholdMin = false;
        bool parsedFxaaSubpixelQuality = false;
        bool parsedEnableFog = false;
        bool parsedEnableVignette = false;
        bool parsedEnableBloom = false;
        bool parsedGrainEnable = false;
        bool parsedGrainAnimate = false;
        bool parsedEnableFxaa = false;

        for (const auto& entry : lines)
        {
            std::string key = TrimWhitespace(entry.key);
            std::string value = TrimWhitespace(entry.value);
            const std::size_t lineNumber = entry.lineNumber;
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c)
            { return static_cast<char>(std::tolower(c)); });

            if (key == "post.gamma")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.gamma = *parsed;
                    parsedGamma = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.gamma=" << settings.gamma
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.gamma value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.gamma << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.saturation")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.saturation = *parsed;
                    parsedSaturation = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.saturation=" << settings.saturation
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.saturation value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.saturation << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.contrast")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.contrast = *parsed;
                    parsedContrast = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.contrast=" << settings.contrast
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.contrast value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.contrast << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.fogcolor")
            {
                if (auto parsed = ParseFloat3Token(value))
                {
                    settings.fogColor = *parsed;
                    parsedFogColor = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.fogColor=("
                        << settings.fogColor.x << ',' << settings.fogColor.y << ',' << settings.fogColor.z
                        << ") from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.fogColor value='" << value
                        << "' on line=" << lineNumber << "; keeping default=("
                        << settings.fogColor.x << ',' << settings.fogColor.y << ',' << settings.fogColor.z << ")\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.fogstart")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.fogStart = *parsed;
                    parsedFogStart = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.fogStart=" << settings.fogStart
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.fogStart value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.fogStart << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.fogrange")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.fogRange = *parsed;
                    parsedFogRange = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.fogRange=" << settings.fogRange
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.fogRange value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.fogRange << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.vignettestrength")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.vignetteStrength = *parsed;
                    parsedVignetteStrength = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.vignetteStrength=" << settings.vignetteStrength
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.vignetteStrength value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.vignetteStrength << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.bloomthreshold")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.bloomThreshold = *parsed;
                    parsedBloomThreshold = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.bloomThreshold=" << settings.bloomThreshold
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.bloomThreshold value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.bloomThreshold << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.bloomintensity")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.bloomIntensity = *parsed;
                    parsedBloomIntensity = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.bloomIntensity=" << settings.bloomIntensity
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.bloomIntensity value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.bloomIntensity << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.grain.amount")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.grainAmount = *parsed;
                    parsedGrainAmount = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.grain.amount=" << settings.grainAmount
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.grain.amount value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.grainAmount << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.fxaa.edgethreshold")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.fxaaEdgeThreshold = *parsed;
                    parsedFxaaEdgeThreshold = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.fxaa.edgeThreshold=" << settings.fxaaEdgeThreshold
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.fxaa.edgeThreshold value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.fxaaEdgeThreshold << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.fxaa.edgethresholdmin")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.fxaaEdgeThresholdMin = *parsed;
                    parsedFxaaEdgeThresholdMin = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.fxaa.edgeThresholdMin=" << settings.fxaaEdgeThresholdMin
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.fxaa.edgeThresholdMin value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.fxaaEdgeThresholdMin << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.fxaa.subpixelquality")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.fxaaSubpixelQuality = *parsed;
                    parsedFxaaSubpixelQuality = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.fxaa.subpixelQuality=" << settings.fxaaSubpixelQuality
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.fxaa.subpixelQuality value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.fxaaSubpixelQuality << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.enablefog")
            {
                bool parsedValue = false;
                if (ParseBool(value, parsedValue))
                {
                    settings.enableFog = parsedValue;
                    parsedEnableFog = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.enableFog=" << std::boolalpha << settings.enableFog
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.enableFog value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << std::boolalpha << settings.enableFog << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.fxaa.enable")
            {
                bool parsedValue = false;
                if (ParseBool(value, parsedValue))
                {
                    settings.enableFxaa = parsedValue;
                    parsedEnableFxaa = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.fxaa.enable=" << std::boolalpha << settings.enableFxaa
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.fxaa.enable value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << std::boolalpha << settings.enableFxaa << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.enablevignette")
            {
                bool parsedValue = false;
                if (ParseBool(value, parsedValue))
                {
                    settings.enableVignette = parsedValue;
                    parsedEnableVignette = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.enableVignette=" << std::boolalpha << settings.enableVignette
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.enableVignette value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << std::boolalpha << settings.enableVignette << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.enablebloom")
            {
                bool parsedValue = false;
                if (ParseBool(value, parsedValue))
                {
                    settings.enableBloom = parsedValue;
                    parsedEnableBloom = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.enableBloom=" << std::boolalpha << settings.enableBloom
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.enableBloom value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << std::boolalpha << settings.enableBloom << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.grain.enable")
            {
                bool parsedValue = false;
                if (ParseBool(value, parsedValue))
                {
                    settings.grainEnable = parsedValue;
                    parsedGrainEnable = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.grain.enable=" << std::boolalpha << settings.grainEnable
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.grain.enable value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << std::boolalpha << settings.grainEnable << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "post.grain.animate")
            {
                bool parsedValue = false;
                if (ParseBool(value, parsedValue))
                {
                    settings.grainAnimate = parsedValue;
                    parsedGrainAnimate = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Parsed post.grain.animate=" << std::boolalpha << settings.grainAnimate
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderSettings] Invalid post.grain.animate value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << std::boolalpha << settings.grainAnimate << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }
        }

        if (!parsedGamma)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.gamma not found; using default=" << settings.gamma
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedSaturation)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.saturation not found; using default=" << settings.saturation
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedContrast)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.contrast not found; using default=" << settings.contrast
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedFogColor)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.fogColor not found; using default=("
                << settings.fogColor.x << ',' << settings.fogColor.y << ',' << settings.fogColor.z
                << ") (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedFogStart)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.fogStart not found; using default=" << settings.fogStart
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedFogRange)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.fogRange not found; using default=" << settings.fogRange
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedVignetteStrength)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.vignetteStrength not found; using default=" << settings.vignetteStrength
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedBloomThreshold)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.bloomThreshold not found; using default=" << settings.bloomThreshold
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedBloomIntensity)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.bloomIntensity not found; using default=" << settings.bloomIntensity
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedGrainAmount)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.grain.amount not found; using default=" << settings.grainAmount
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedFxaaEdgeThreshold)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.fxaa.edgeThreshold not found; using default=" << settings.fxaaEdgeThreshold
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedFxaaEdgeThresholdMin)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.fxaa.edgeThresholdMin not found; using default=" << settings.fxaaEdgeThresholdMin
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedFxaaSubpixelQuality)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.fxaa.subpixelQuality not found; using default=" << settings.fxaaSubpixelQuality
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedEnableFog)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.enableFog not found; using default=" << std::boolalpha << settings.enableFog
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedEnableVignette)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.enableVignette not found; using default="
                << std::boolalpha << settings.enableVignette << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedEnableBloom)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.enableBloom not found; using default=" << std::boolalpha << settings.enableBloom
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedGrainEnable)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.grain.enable not found; using default=" << std::boolalpha << settings.grainEnable
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedGrainAnimate)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.grain.animate not found; using default="
                << std::boolalpha << settings.grainAnimate << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedEnableFxaa)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderSettings] post.fxaa.enable not found; using default=" << std::boolalpha << settings.enableFxaa
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }

        return settings;
    }

    RenderFadeSettings LoadRenderFadeSettings()
    {
        RenderFadeSettings settings{};
        std::vector<Config::SectionLine> lines;
        if (!Config::ReadSectionLines(kTechnicalConfigPath, kRenderFadeSection, lines))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderFade] Missing config; defaulting fade settings section='"
                << kRenderFadeSection << "' path='" << kTechnicalConfigPath << "'\n";
            DebugTrace(oss.str());
            return settings;
        }

        bool parsedEnable = false;
        bool parsedFadeIn = false;
        bool parsedFadeOut = false;
        bool parsedHold = false;
        bool parsedColor = false;
        bool parsedEasing = false;

        for (const auto& entry : lines)
        {
            std::string key = TrimWhitespace(entry.key);
            std::string value = TrimWhitespace(entry.value);
            const std::size_t lineNumber = entry.lineNumber;
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c)
            { return static_cast<char>(std::tolower(c)); });

            if (key == "enable")
            {
                bool parsedValue = false;
                if (ParseBool(value, parsedValue))
                {
                    settings.enable = parsedValue;
                    parsedEnable = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderFade] Parsed enable=" << std::boolalpha << settings.enable
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderFade] Invalid enable value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << std::boolalpha << settings.enable << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "fadeinseconds")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.fadeInSeconds = std::max(0.0f, *parsed);
                    parsedFadeIn = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderFade] Parsed fadeInSeconds=" << settings.fadeInSeconds
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderFade] Invalid fadeInSeconds value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.fadeInSeconds << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "fadeoutseconds")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.fadeOutSeconds = std::max(0.0f, *parsed);
                    parsedFadeOut = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderFade] Parsed fadeOutSeconds=" << settings.fadeOutSeconds
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderFade] Invalid fadeOutSeconds value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.fadeOutSeconds << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "holdseconds")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.holdSeconds = std::max(0.0f, *parsed);
                    parsedHold = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderFade] Parsed holdSeconds=" << settings.holdSeconds
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderFade] Invalid holdSeconds value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.holdSeconds << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "color")
            {
                if (auto parsed = ParseFloat3Token(value))
                {
                    settings.color = *parsed;
                    parsedColor = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderFade] Parsed color=("
                        << settings.color.x << ',' << settings.color.y << ',' << settings.color.z
                        << ") from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderFade] Invalid color value='" << value
                        << "' on line=" << lineNumber << "; keeping default=("
                        << settings.color.x << ',' << settings.color.y << ',' << settings.color.z << ")\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "easing")
            {
                if (auto parsed = ParseRenderFadeEasing(value))
                {
                    settings.easing = *parsed;
                    parsedEasing = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderFade] Parsed easing='" << value
                        << "' from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[RenderFade] Invalid easing value='" << value
                        << "' on line=" << lineNumber << "; keeping default\n";
                    DebugTrace(oss.str());
                }
                continue;
            }
        }

        if (!parsedEnable)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderFade] enable not found; using default=" << std::boolalpha << settings.enable
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedFadeIn)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderFade] fadeInSeconds not found; using default=" << settings.fadeInSeconds
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedFadeOut)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderFade] fadeOutSeconds not found; using default=" << settings.fadeOutSeconds
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedHold)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderFade] holdSeconds not found; using default=" << settings.holdSeconds
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedColor)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderFade] color not found; using default=("
                << settings.color.x << ',' << settings.color.y << ',' << settings.color.z
                << ") (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedEasing)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[RenderFade] easing not found; using default (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }

        return settings;
    }

    LightingSettings LoadLightingSettings()
    {
        LightingSettings settings{};
        std::vector<Config::SectionLine> lines;
        if (!Config::ReadSectionLines(kTechnicalConfigPath, kRenderSettingsSection, lines))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[LightingSettings] Missing config; defaulting lighting settings section='"
                << kRenderSettingsSection << "' path='" << kTechnicalConfigPath << "'\n";
            DebugTrace(oss.str());
            return settings;
        }

        bool parsedMode = false;
        bool parsedKeyDiffuse = false;
        bool parsedKeyDir = false;
        bool parsedKeyWrap = false;
        bool parsedKeyEnable = false;
        bool parsedFillDiffuse = false;
        bool parsedFillDir = false;
        bool parsedFillWrap = false;
        bool parsedFillEnable = false;
        bool parsedHemiSky = false;
        bool parsedHemiGround = false;
        bool parsedRimColor = false;
        bool parsedRimParams = false;
        bool parsedRimEnable = false;

        for (const auto& entry : lines)
        {
            std::string key = TrimWhitespace(entry.key);
            std::string value = TrimWhitespace(entry.value);
            const std::size_t lineNumber = entry.lineNumber;
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c)
            { return static_cast<char>(std::tolower(c)); });

            if (key == "lighting.mode")
            {
                bool parsedValue = false;
                std::string modeToken = value;
                std::transform(modeToken.begin(), modeToken.end(), modeToken.begin(), [](unsigned char c)
                { return static_cast<char>(std::tolower(c)); });

                if (modeToken == "lambert")
                {
                    settings.mode = 0;
                    parsedValue = true;
                }
                else if (modeToken == "stylized")
                {
                    settings.mode = 1;
                    parsedValue = true;
                }
                else if (auto parsed = TryParseIntToken(value))
                {
                    settings.mode = (*parsed == 0) ? 0 : 1;
                    parsedValue = true;
                }
                else if (auto parsedFloat = TryParseFloatToken(value))
                {
                    settings.mode = (*parsedFloat <= 0.0f) ? 0 : 1;
                    parsedValue = true;
                }

                if (parsedValue)
                {
                    parsedMode = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Parsed lighting.mode=" << settings.mode
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Invalid lighting.mode value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.mode << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "lighting.key.diffuse" || key == "light.key.diffuse")
            {
                if (auto parsed = ParseFloat4Token(value, settings.keyDiffuse.w))
                {
                    settings.keyDiffuse = *parsed;
                    parsedKeyDiffuse = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Parsed " << key << "=("
                        << settings.keyDiffuse.x << ',' << settings.keyDiffuse.y << ','
                        << settings.keyDiffuse.z << ',' << settings.keyDiffuse.w
                        << ") from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Invalid " << key << " value='" << value
                        << "' on line=" << lineNumber << "; keeping default=("
                        << settings.keyDiffuse.x << ',' << settings.keyDiffuse.y << ','
                        << settings.keyDiffuse.z << ',' << settings.keyDiffuse.w << ")\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "lighting.key.dir" || key == "light.key.dir")
            {
                if (auto parsed = ParseFloat3Token(value))
                {
                    settings.keyDir = DirectX::XMFLOAT3(parsed->x, parsed->y, parsed->z);
                    parsedKeyDir = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Parsed " << key << "=("
                        << settings.keyDir.x << ',' << settings.keyDir.y << ',' << settings.keyDir.z
                        << ") from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Invalid " << key << " value='" << value
                        << "' on line=" << lineNumber << "; keeping default=("
                        << settings.keyDir.x << ',' << settings.keyDir.y << ',' << settings.keyDir.z << ")\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "lighting.key.wrap" || key == "light.key.wrap")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.keyWrap = *parsed;
                    parsedKeyWrap = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Parsed " << key << "=" << settings.keyWrap
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Invalid " << key << " value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.keyWrap << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "lighting.key.enable" || key == "light.key.enable")
            {
                bool parsedValue = false;
                if (ParseBool(value, settings.keyEnable))
                {
                    parsedValue = true;
                }

                if (parsedValue)
                {
                    parsedKeyEnable = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Parsed " << key << "=" << std::boolalpha << settings.keyEnable
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Invalid " << key << " value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << std::boolalpha << settings.keyEnable << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "lighting.fill.diffuse" || key == "light.fill.diffuse")
            {
                if (auto parsed = ParseFloat4Token(value, settings.fillDiffuse.w))
                {
                    settings.fillDiffuse = *parsed;
                    parsedFillDiffuse = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Parsed " << key << "=("
                        << settings.fillDiffuse.x << ',' << settings.fillDiffuse.y << ','
                        << settings.fillDiffuse.z << ',' << settings.fillDiffuse.w
                        << ") from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Invalid " << key << " value='" << value
                        << "' on line=" << lineNumber << "; keeping default=("
                        << settings.fillDiffuse.x << ',' << settings.fillDiffuse.y << ','
                        << settings.fillDiffuse.z << ',' << settings.fillDiffuse.w << ")\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "lighting.fill.dir" || key == "light.fill.dir")
            {
                if (auto parsed = ParseFloat3Token(value))
                {
                    settings.fillDir = DirectX::XMFLOAT3(parsed->x, parsed->y, parsed->z);
                    parsedFillDir = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Parsed " << key << "=("
                        << settings.fillDir.x << ',' << settings.fillDir.y << ',' << settings.fillDir.z
                        << ") from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Invalid " << key << " value='" << value
                        << "' on line=" << lineNumber << "; keeping default=("
                        << settings.fillDir.x << ',' << settings.fillDir.y << ',' << settings.fillDir.z << ")\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "lighting.fill.wrap" || key == "light.fill.wrap")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.fillWrap = *parsed;
                    parsedFillWrap = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Parsed " << key << "=" << settings.fillWrap
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Invalid " << key << " value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.fillWrap << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "lighting.fill.enable" || key == "light.fill.enable")
            {
                bool parsedValue = false;
                if (ParseBool(value, settings.fillEnable))
                {
                    parsedValue = true;
                }

                if (parsedValue)
                {
                    parsedFillEnable = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Parsed " << key << "=" << std::boolalpha << settings.fillEnable
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Invalid " << key << " value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << std::boolalpha << settings.fillEnable << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "lighting.directional.enable" || key == "light.directional.enable")
            {
                bool parsedValue = false;
                bool directionalEnable = true;
                if (ParseBool(value, directionalEnable))
                {
                    settings.keyEnable = directionalEnable;
                    settings.fillEnable = directionalEnable;
                    parsedValue = true;
                }

                if (parsedValue)
                {
                    parsedKeyEnable = true;
                    parsedFillEnable = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Parsed " << key << "=" << std::boolalpha << directionalEnable
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Invalid " << key << " value='" << value
                        << "' on line=" << lineNumber << "; keeping defaults keyEnable=" << std::boolalpha << settings.keyEnable
                        << " fillEnable=" << settings.fillEnable << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "lighting.hemi.sky" || key == "light.hemi.sky")
            {
                if (auto parsed = ParseFloat4Token(value, settings.hemiSky.w))
                {
                    settings.hemiSky = *parsed;
                    parsedHemiSky = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Parsed " << key << "=("
                        << settings.hemiSky.x << ',' << settings.hemiSky.y << ','
                        << settings.hemiSky.z << ',' << settings.hemiSky.w
                        << ") from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Invalid " << key << " value='" << value
                        << "' on line=" << lineNumber << "; keeping default=("
                        << settings.hemiSky.x << ',' << settings.hemiSky.y << ','
                        << settings.hemiSky.z << ',' << settings.hemiSky.w << ")\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "lighting.hemi.ground" || key == "light.hemi.ground")
            {
                if (auto parsed = ParseFloat4Token(value, settings.hemiGround.w))
                {
                    settings.hemiGround = *parsed;
                    parsedHemiGround = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Parsed " << key << "=("
                        << settings.hemiGround.x << ',' << settings.hemiGround.y << ','
                        << settings.hemiGround.z << ',' << settings.hemiGround.w
                        << ") from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Invalid " << key << " value='" << value
                        << "' on line=" << lineNumber << "; keeping default=("
                        << settings.hemiGround.x << ',' << settings.hemiGround.y << ','
                        << settings.hemiGround.z << ',' << settings.hemiGround.w << ")\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "lighting.rim.color" || key == "light.rim.color")
            {
                if (auto parsed = ParseFloat4Token(value, settings.rimColor.w))
                {
                    settings.rimColor = *parsed;
                    parsedRimColor = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Parsed " << key << "=("
                        << settings.rimColor.x << ',' << settings.rimColor.y << ','
                        << settings.rimColor.z << ',' << settings.rimColor.w
                        << ") from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Invalid " << key << " value='" << value
                        << "' on line=" << lineNumber << "; keeping default=("
                        << settings.rimColor.x << ',' << settings.rimColor.y << ','
                        << settings.rimColor.z << ',' << settings.rimColor.w << ")\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "lighting.rim.params" || key == "light.rim.params")
            {
                if (auto parsed = ParseFloat4Token(value, settings.rimParams.w))
                {
                    settings.rimParams = *parsed;
                    parsedRimParams = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Parsed " << key << "=("
                        << settings.rimParams.x << ',' << settings.rimParams.y << ','
                        << settings.rimParams.z << ',' << settings.rimParams.w
                        << ") from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Invalid " << key << " value='" << value
                        << "' on line=" << lineNumber << "; keeping default=("
                        << settings.rimParams.x << ',' << settings.rimParams.y << ','
                        << settings.rimParams.z << ',' << settings.rimParams.w << ")\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "lighting.rim.enable" || key == "light.rim.enable")
            {
                bool parsedValue = false;
                if (ParseBool(value, settings.rimEnable))
                {
                    parsedValue = true;
                }

                if (parsedValue)
                {
                    parsedRimEnable = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Parsed " << key << "=" << std::boolalpha << settings.rimEnable
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[LightingSettings] Invalid " << key << " value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << std::boolalpha << settings.rimEnable << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }
        }

        if (!parsedMode)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[LightingSettings] lighting.mode not found; using default=" << settings.mode
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedKeyDiffuse)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[LightingSettings] lighting.key.diffuse not found; using default=("
                << settings.keyDiffuse.x << ',' << settings.keyDiffuse.y << ','
                << settings.keyDiffuse.z << ',' << settings.keyDiffuse.w
                << ") (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedKeyDir)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[LightingSettings] lighting.key.dir not found; using default=("
                << settings.keyDir.x << ',' << settings.keyDir.y << ',' << settings.keyDir.z
                << ") (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedFillDiffuse)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[LightingSettings] lighting.fill.diffuse not found; using default=("
                << settings.fillDiffuse.x << ',' << settings.fillDiffuse.y << ','
                << settings.fillDiffuse.z << ',' << settings.fillDiffuse.w
                << ") (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedFillDir)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[LightingSettings] lighting.fill.dir not found; using default=("
                << settings.fillDir.x << ',' << settings.fillDir.y << ',' << settings.fillDir.z
                << ") (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedKeyWrap)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[LightingSettings] lighting.key.wrap not found; using default=" << settings.keyWrap
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedFillWrap)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[LightingSettings] lighting.fill.wrap not found; using default=" << settings.fillWrap
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedKeyEnable)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[LightingSettings] lighting.key.enable not found; using default="
                << std::boolalpha << settings.keyEnable << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedFillEnable)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[LightingSettings] lighting.fill.enable not found; using default="
                << std::boolalpha << settings.fillEnable << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedHemiSky)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[LightingSettings] lighting.hemi.sky not found; using default=("
                << settings.hemiSky.x << ',' << settings.hemiSky.y << ','
                << settings.hemiSky.z << ',' << settings.hemiSky.w
                << ") (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedHemiGround)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[LightingSettings] lighting.hemi.ground not found; using default=("
                << settings.hemiGround.x << ',' << settings.hemiGround.y << ','
                << settings.hemiGround.z << ',' << settings.hemiGround.w
                << ") (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedRimColor)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[LightingSettings] lighting.rim.color not found; using default=("
                << settings.rimColor.x << ',' << settings.rimColor.y << ','
                << settings.rimColor.z << ',' << settings.rimColor.w
                << ") (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedRimParams)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[LightingSettings] lighting.rim.params not found; using default=("
                << settings.rimParams.x << ',' << settings.rimParams.y << ','
                << settings.rimParams.z << ',' << settings.rimParams.w
                << ") (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedRimEnable)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[LightingSettings] lighting.rim.enable not found; using default="
                << std::boolalpha << settings.rimEnable << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }

        return settings;
    }

    BlobShadowSettings LoadBlobShadowSettings()
    {
        BlobShadowSettings settings{};
        std::vector<Config::SectionLine> lines;
        if (!Config::ReadSectionLines(kTechnicalConfigPath, kRenderSettingsSection, lines))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[BlobShadow] Missing config; defaulting blob shadow settings section='"
                << kRenderSettingsSection << "' path='" << kTechnicalConfigPath << "'\n";
            DebugTrace(oss.str());
            return settings;
        }

        bool parsedEnable = false;
        bool parsedDebugNoDepthTest = false;
        bool parsedForceReceiverZ = false;
        bool parsedOpacity = false;
        bool parsedSoftness = false;
        bool parsedMaxHeight = false;
        bool parsedMinScale = false;
        bool parsedReceiverBias = false;
        bool parsedGroundY = false;
        bool parsedGroundZ = false;

        for (const auto& entry : lines)
        {
            std::string key = TrimWhitespace(entry.key);
            std::string value = TrimWhitespace(entry.value);
            const std::size_t lineNumber = entry.lineNumber;
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c)
            { return static_cast<char>(std::tolower(c)); });

            if (key == "shadow.blob.enable")
            {
                if (ParseBool(value, settings.enable))
                {
                    parsedEnable = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Parsed shadow.blob.enable=" << std::boolalpha << settings.enable
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Invalid shadow.blob.enable value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << std::boolalpha << settings.enable << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "shadow.blob.debugnodepthtest")
            {
                if (ParseBool(value, settings.debugNoDepthTest))
                {
                    parsedDebugNoDepthTest = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Parsed shadow.blob.debugNoDepthTest="
                        << std::boolalpha << settings.debugNoDepthTest
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Invalid shadow.blob.debugNoDepthTest value='" << value
                        << "' on line=" << lineNumber << "; keeping default="
                        << std::boolalpha << settings.debugNoDepthTest << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "shadow.blob.forcereceiverz")
            {
                if (ParseBool(value, settings.forceReceiverZ))
                {
                    parsedForceReceiverZ = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Parsed shadow.blob.forceReceiverZ="
                        << std::boolalpha << settings.forceReceiverZ
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Invalid shadow.blob.forceReceiverZ value='" << value
                        << "' on line=" << lineNumber << "; keeping default="
                        << std::boolalpha << settings.forceReceiverZ << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "shadow.blob.opacity")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.opacity = std::clamp(*parsed, 0.0f, 1.0f);
                    parsedOpacity = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Parsed shadow.blob.opacity=" << settings.opacity
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Invalid shadow.blob.opacity value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.opacity << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "shadow.blob.softness")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.softness = std::clamp(*parsed, 0.0f, 1.0f);
                    parsedSoftness = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Parsed shadow.blob.softness=" << settings.softness
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Invalid shadow.blob.softness value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.softness << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "shadow.blob.maxheight")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.maxHeight = std::max(0.0f, *parsed);
                    parsedMaxHeight = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Parsed shadow.blob.maxHeight=" << settings.maxHeight
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Invalid shadow.blob.maxHeight value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.maxHeight << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "shadow.blob.minscale")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.minScale = std::clamp(*parsed, 0.0f, 1.0f);
                    parsedMinScale = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Parsed shadow.blob.minScale=" << settings.minScale
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Invalid shadow.blob.minScale value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.minScale << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "shadow.blob.receiverbias")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.receiverBias = *parsed;
                    parsedReceiverBias = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Parsed shadow.blob.receiverBias=" << settings.receiverBias
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Invalid shadow.blob.receiverBias value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.receiverBias << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "shadow.blob.groundy")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.groundY = *parsed;
                    parsedGroundY = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Parsed shadow.blob.groundY=" << settings.groundY
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Invalid shadow.blob.groundY value='" << value
                        << "' on line=" << lineNumber << "; keeping default=" << settings.groundY << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (key == "shadow.blob.groundz")
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.groundZ = *parsed;
                    parsedGroundZ = true;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Parsed shadow.blob.groundZ=" << *settings.groundZ
                        << " from line=" << lineNumber << " path='" << kTechnicalConfigPath << "'\n";
                    DebugTrace(oss.str());
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[BlobShadow] Invalid shadow.blob.groundZ value='" << value
                        << "' on line=" << lineNumber << "; keeping default="
                        << (settings.groundZ ? std::to_string(*settings.groundZ) : std::string("unset")) << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }
        }

        if (!parsedEnable)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[BlobShadow] shadow.blob.enable not found; using default="
                << std::boolalpha << settings.enable << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedOpacity)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[BlobShadow] shadow.blob.opacity not found; using default=" << settings.opacity
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedDebugNoDepthTest)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[BlobShadow] shadow.blob.debugNoDepthTest not found; using default="
                << std::boolalpha << settings.debugNoDepthTest
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedForceReceiverZ)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[BlobShadow] shadow.blob.forceReceiverZ not found; using default="
                << std::boolalpha << settings.forceReceiverZ
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedSoftness)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[BlobShadow] shadow.blob.softness not found; using default=" << settings.softness
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedMaxHeight)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[BlobShadow] shadow.blob.maxHeight not found; using default=" << settings.maxHeight
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedMinScale)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[BlobShadow] shadow.blob.minScale not found; using default=" << settings.minScale
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedReceiverBias)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[BlobShadow] shadow.blob.receiverBias not found; using default=" << settings.receiverBias
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedGroundY)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[BlobShadow] shadow.blob.groundY not found; using default=" << settings.groundY
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }
        if (!parsedGroundZ)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[BlobShadow] shadow.blob.groundZ not found; using default="
                << (settings.groundZ ? std::to_string(*settings.groundZ) : std::string("unset"))
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }

        return settings;
    }

    MaterialTuningSettings LoadMaterialTuningSettings()
    {
        MaterialTuningSettings settings{};
        std::vector<Config::SectionLine> lines;
        if (!Config::ReadSectionLines(kTechnicalConfigPath, kRenderSettingsSection, lines))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "[MaterialTuning] Missing config; defaulting material tuning section='"
                << kRenderSettingsSection << "' path='" << kTechnicalConfigPath << "'\n";
            DebugTrace(oss.str());
            return settings;
        }

        constexpr std::string_view kProfilePrefix = "material.profile.";
        constexpr std::string_view kOverridePrefix = "material.override.";
        constexpr std::string_view kProfileSuffix = ".profile";

        for (const auto& entry : lines)
        {
            std::string key = TrimWhitespace(entry.key);
            std::string value = TrimWhitespace(entry.value);
            const std::size_t lineNumber = entry.lineNumber;
            if (key.empty())
            {
                continue;
            }

            std::string loweredKey = ToLowerCopy(key);
            if (StartsWith(loweredKey, std::string(kProfilePrefix)))
            {
                const std::string remainder = loweredKey.substr(kProfilePrefix.size());
                const std::size_t dotPos = remainder.find('.');
                if (dotPos == std::string::npos || dotPos == 0 || dotPos + 1 >= remainder.size())
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[MaterialTuning] Invalid profile key '" << key
                        << "' on line=" << lineNumber << "\n";
                    DebugTrace(oss.str());
                    continue;
                }

                const std::string profileName = remainder.substr(0, dotPos);
                const std::string property = remainder.substr(dotPos + 1);
                MaterialTuningProfile& profile = settings.profiles[profileName];

                if (property == "lightresponse")
                {
                    if (auto parsed = TryParseFloatToken(value))
                    {
                        profile.lightResponse = *parsed;
                        std::ostringstream oss;
                        oss << kLogPrefix << "[MaterialTuning] Parsed material.profile." << profileName
                            << ".lightResponse=" << profile.lightResponse << " from line=" << lineNumber << "\n";
                        DebugTrace(oss.str());
                    }
                    else
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "[MaterialTuning] Invalid material.profile." << profileName
                            << ".lightResponse value='" << value << "' on line=" << lineNumber << "\n";
                        DebugTrace(oss.str());
                    }
                }
                else if (property == "sssintensity")
                {
                    if (auto parsed = TryParseFloatToken(value))
                    {
                        profile.sssIntensity = *parsed;
                        std::ostringstream oss;
                        oss << kLogPrefix << "[MaterialTuning] Parsed material.profile." << profileName
                            << ".sssIntensity=" << profile.sssIntensity << " from line=" << lineNumber << "\n";
                        DebugTrace(oss.str());
                    }
                    else
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "[MaterialTuning] Invalid material.profile." << profileName
                            << ".sssIntensity value='" << value << "' on line=" << lineNumber << "\n";
                        DebugTrace(oss.str());
                    }
                }
                else if (property == "ssscolor")
                {
                    if (auto parsed = ParseFloat3Token(value))
                    {
                        profile.sssColor = DirectX::XMFLOAT3(parsed->x, parsed->y, parsed->z);
                        std::ostringstream oss;
                        oss << kLogPrefix << "[MaterialTuning] Parsed material.profile." << profileName
                            << ".sssColor=(" << profile.sssColor.x << ',' << profile.sssColor.y << ','
                            << profile.sssColor.z << ") from line=" << lineNumber << "\n";
                        DebugTrace(oss.str());
                    }
                    else
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "[MaterialTuning] Invalid material.profile." << profileName
                            << ".sssColor value='" << value << "' on line=" << lineNumber << "\n";
                        DebugTrace(oss.str());
                    }
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[MaterialTuning] Unknown profile property '" << property
                        << "' in key='" << key << "' on line=" << lineNumber << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }

            if (StartsWith(loweredKey, std::string(kOverridePrefix)))
            {
                std::string remainder = loweredKey.substr(kOverridePrefix.size());
                std::string target = remainder;
                std::string property = "profile";
                const std::size_t dotPos = remainder.rfind('.');
                if (dotPos != std::string::npos && dotPos + 1 < remainder.size())
                {
                    const std::string candidateProperty = remainder.substr(dotPos + 1);
                    if (candidateProperty == "profile"
                        || candidateProperty == "lightresponse"
                        || candidateProperty == "sssintensity"
                        || candidateProperty == "ssscolor")
                    {
                        property = candidateProperty;
                        target = remainder.substr(0, dotPos);
                    }
                }
                else if (EndsWith(target, std::string(kProfileSuffix)))
                {
                    target = target.substr(0, target.size() - kProfileSuffix.size());
                }

                if (target.empty())
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[MaterialTuning] Invalid override key '" << key
                        << "' on line=" << lineNumber << "\n";
                    DebugTrace(oss.str());
                    continue;
                }

                MaterialTuningOverride& overrideEntry = settings.overrides[target];
                if (property == "profile")
                {
                    const std::string profileName = ToLowerCopy(value);
                    if (profileName.empty())
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "[MaterialTuning] Empty override profile for key '" << key
                            << "' on line=" << lineNumber << "\n";
                        DebugTrace(oss.str());
                        continue;
                    }

                    overrideEntry.profileName = profileName;
                    std::ostringstream oss;
                    oss << kLogPrefix << "[MaterialTuning] Parsed material.override." << target
                        << ".profile='" << profileName << "' from line=" << lineNumber << "\n";
                    DebugTrace(oss.str());
                }
                else if (property == "lightresponse")
                {
                    if (auto parsed = TryParseFloatToken(value))
                    {
                        overrideEntry.lightResponse = *parsed;
                        std::ostringstream oss;
                        oss << kLogPrefix << "[MaterialTuning] Parsed material.override." << target
                            << ".lightResponse=" << *overrideEntry.lightResponse
                            << " from line=" << lineNumber << "\n";
                        DebugTrace(oss.str());
                    }
                    else
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "[MaterialTuning] Invalid material.override." << target
                            << ".lightResponse value='" << value << "' on line=" << lineNumber << "\n";
                        DebugTrace(oss.str());
                    }
                }
                else if (property == "sssintensity")
                {
                    if (auto parsed = TryParseFloatToken(value))
                    {
                        overrideEntry.sssIntensity = *parsed;
                        std::ostringstream oss;
                        oss << kLogPrefix << "[MaterialTuning] Parsed material.override." << target
                            << ".sssIntensity=" << *overrideEntry.sssIntensity
                            << " from line=" << lineNumber << "\n";
                        DebugTrace(oss.str());
                    }
                    else
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "[MaterialTuning] Invalid material.override." << target
                            << ".sssIntensity value='" << value << "' on line=" << lineNumber << "\n";
                        DebugTrace(oss.str());
                    }
                }
                else if (property == "ssscolor")
                {
                    if (auto parsed = ParseFloat3Token(value))
                    {
                        overrideEntry.sssColor = DirectX::XMFLOAT3(parsed->x, parsed->y, parsed->z);
                        std::ostringstream oss;
                        oss << kLogPrefix << "[MaterialTuning] Parsed material.override." << target
                            << ".sssColor=(" << overrideEntry.sssColor->x << ',' << overrideEntry.sssColor->y
                            << ',' << overrideEntry.sssColor->z << ") from line=" << lineNumber << "\n";
                        DebugTrace(oss.str());
                    }
                    else
                    {
                        std::ostringstream oss;
                        oss << kLogPrefix << "[MaterialTuning] Invalid material.override." << target
                            << ".sssColor value='" << value << "' on line=" << lineNumber << "\n";
                        DebugTrace(oss.str());
                    }
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "[MaterialTuning] Unknown override property '" << property
                        << "' in key='" << key << "' on line=" << lineNumber << "\n";
                    DebugTrace(oss.str());
                }
                continue;
            }
        }

        {
            std::ostringstream oss;
            oss << kLogPrefix << "[MaterialTuning] Profiles=" << settings.profiles.size()
                << " overrides=" << settings.overrides.size()
                << " (path='" << kTechnicalConfigPath << "')\n";
            DebugTrace(oss.str());
        }

        return settings;
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

    CameraRigSettings LoadCameraRigSettings()
    {
        CameraRigSettings settings{};

        std::vector<Config::SectionLine> lines;
        if (!Config::ReadSectionLines(kGameplayConfigPath, GameplayConfig::kSectionCameraRig, lines))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Camera rig config section '" << GameplayConfig::kSectionCameraRig
                << "' not found at '" << kGameplayConfigPath << "'.\n";
            ::DebugTrace(oss.str());
            return settings;
        }

        for (const auto& entry : lines)
        {
            const std::string key = entry.key;
            const std::string value = TrimWhitespace(entry.value);
            if (key == GameplayConfig::kCameraRigKeyMode)
            {
                if (auto parsed = ParseCameraRigMode(value))
                {
                    settings.mode = *parsed;
                }
                else
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Unknown camera rig mode '" << value
                        << "'; using default.\n";
                    ::DebugTrace(oss.str());
                }
            }
            else if (key == GameplayConfig::kCameraRigKeyMinY)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.clamp.minY = *parsed;
                }
            }
            else if (key == GameplayConfig::kCameraRigKeyMaxY)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.clamp.maxY = *parsed;
                }
            }
            else if (key == GameplayConfig::kCameraRigKeyFollowSpeed)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.followSpeed = std::max(0.0f, *parsed);
                }
            }
            else if (key == GameplayConfig::kCameraRigKeyOffset)
            {
                if (auto parsed = ParseFloat3Token(value))
                {
                    settings.offset = *parsed;
                }
            }
            else if (key == GameplayConfig::kCameraRigKeyFovDegrees)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.fovDegrees = *parsed;
                }
            }
            else if (key == GameplayConfig::kCameraRigKeyYawDegrees)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.yawDegrees = *parsed;
                }
            }
            else if (key == GameplayConfig::kCameraRigKeyPitchDegrees)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.pitchDegrees = *parsed;
                }
            }
        }

        if (settings.clamp.minY && settings.clamp.maxY && *settings.clamp.minY > *settings.clamp.maxY)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Camera rig clamp range invalid (minY=" << *settings.clamp.minY
                << ", maxY=" << *settings.clamp.maxY << "). Swapping values.\n";
            ::DebugTrace(oss.str());
            std::swap(settings.clamp.minY, settings.clamp.maxY);
        }

        return settings;
    }

    GameCameraConfig LoadGameCameraConfig()
    {
        GameCameraConfig config{};

        std::vector<Config::SectionLine> lines;
        if (!Config::ReadSectionLines(kGameplayConfigPath, GameplayConfig::kSectionCameraRig, lines))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Game camera config section '" << GameplayConfig::kSectionCameraRig
                << "' not found at '" << kGameplayConfigPath << "'.\n";
            ::DebugTrace(oss.str());
            return config;
        }

        for (const auto& entry : lines)
        {
            const std::string key = entry.key;
            const std::string value = TrimWhitespace(entry.value);
            if (key == GameplayConfig::kCameraRigKeyComfortZoneTopPercentY)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    config.comfortZoneTopPercentY = std::clamp(*parsed, 0.0f, 1.0f);
                }
            }
            else if (key == GameplayConfig::kCameraRigKeyComfortZoneBottomPercentY)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    config.comfortZoneBottomPercentY = std::clamp(*parsed, 0.0f, 1.0f);
                }
            }
            else if (key == GameplayConfig::kCameraRigKeyFollowSpeedY)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    config.followSpeedY = std::max(0.0f, *parsed);
                }
            }
            else if (key == GameplayConfig::kCameraRigKeyOffsetY)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    config.offsetY = *parsed;
                }
            }
        }

        return config;
    }

    CreamAdhesionSettings LoadCreamAdhesionSettings()
    {
        CreamAdhesionSettings settings{};
        settings.adhesionDelaySeconds = 0.3f;
        settings.adhesionGraceSeconds = 0.08f;
        settings.breakImpulseThreshold = 6.0f;
        settings.releaseCooldownSeconds = 0.0f;
        settings.allowedTags = { "strawberry", "kiwi", "mikan", "muscat", "melon", "blueberry", "pineapple", "banana", "cake" };
        settings.blockedTags = { "osara", "fairy" };
        settings.requireSupport = false;
        settings.requireCarried = false;
        settings.allowAirAdhesionToSweets = true;
        settings.weldLinearHertz = 6.0f;
        settings.weldAngularHertz = 6.0f;
        settings.weldLinearDampingRatio = 0.7f;
        settings.weldAngularDampingRatio = 0.7f;
        settings.breakImpulseThreshold = 0.0f;

        std::vector<Config::SectionLine> lines;
        if (!Config::ReadSectionLines(kGameplayConfigPath, GameplayConfig::kSectionCream, lines))
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Cream adhesion config section '" << GameplayConfig::kSectionCream
                << "' not found at '" << kGameplayConfigPath << "'. Using defaults.\n";
            ::DebugTrace(oss.str());
            return settings;
        }

        for (const auto& entry : lines)
        {
            const std::string key = TrimWhitespace(entry.key);
            const std::string value = TrimWhitespace(entry.value);
            if (key.empty() || value.empty())
            {
                continue;
            }

            if (key == GameplayConfig::kCreamKeyAdhesionDelaySeconds)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.adhesionDelaySeconds = std::max(0.0f, *parsed);
                }
            }
            else if (key == GameplayConfig::kCreamKeyAdhesionGraceSeconds)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.adhesionGraceSeconds = std::max(0.0f, *parsed);
                }
            }
            else if (key == GameplayConfig::kCreamKeyBreakImpulseThreshold)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.breakImpulseThreshold = std::max(0.0f, *parsed);
                }
            }
            else if (key == GameplayConfig::kCreamKeyReleaseCooldownSeconds)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.releaseCooldownSeconds = std::max(0.0f, *parsed);
                }
            }
            else if (key == GameplayConfig::kCreamKeyAllowedTags)
            {
                settings.allowedTags = ParseTagList(value);
            }
            else if (key == GameplayConfig::kCreamKeyBlockedTags)
            {
                settings.blockedTags = ParseTagList(value);
            }
            else if (key == GameplayConfig::kCreamKeyRequireSupport)
            {
                if (!ParseBool(value, settings.requireSupport))
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Invalid bool for key '" << key << "' in cream config.\n";
                    ::DebugTrace(oss.str());
                }
            }
            else if (key == GameplayConfig::kCreamKeyRequireCarried)
            {
                if (!ParseBool(value, settings.requireCarried))
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Invalid bool for key '" << key << "' in cream config.\n";
                    ::DebugTrace(oss.str());
                }
            }
            else if (key == GameplayConfig::kCreamKeyAllowAirAdhesionToSweets)
            {
                if (!ParseBool(value, settings.allowAirAdhesionToSweets))
                {
                    std::ostringstream oss;
                    oss << kLogPrefix << "Invalid bool for key '" << key << "' in cream config.\n";
                    ::DebugTrace(oss.str());
                }
            }
            else if (key == GameplayConfig::kCreamKeyWeldLinearHertz)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.weldLinearHertz = std::max(0.0f, *parsed);
                }
            }
            else if (key == GameplayConfig::kCreamKeyWeldAngularHertz)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.weldAngularHertz = std::max(0.0f, *parsed);
                }
            }
            else if (key == GameplayConfig::kCreamKeyWeldLinearDampingRatio)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.weldLinearDampingRatio = std::max(0.0f, *parsed);
                }
            }
            else if (key == GameplayConfig::kCreamKeyWeldAngularDampingRatio)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.weldAngularDampingRatio = std::max(0.0f, *parsed);
                }
            }
            else if (key == GameplayConfig::kCreamKeyBreakImpulseThreshold)
            {
                if (auto parsed = TryParseFloatToken(value))
                {
                    settings.breakImpulseThreshold = std::max(0.0f, *parsed);
                }
            }
        }

        return settings;
    }
}
