#include "ColliderConfigLoader.h"

#include "ConfigSectionReader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace ColliderConfigLoaderLogging
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

        void LogWarning(const std::string& message)
        {
                std::cerr << message;
                DebugTrace(message);
        }
}

namespace
{
        constexpr const char* kLogPrefix = "[ColliderConfig] ";
        namespace Logging = ColliderConfigLoaderLogging;

        std::string Trim(const std::string& value)
        {
                auto begin = value.begin();
                while (begin != value.end() && std::isspace(static_cast<unsigned char>(*begin)))
                {
                        ++begin;
                }

                auto end = value.end();
                while (end != begin)
                {
                        auto prev = end;
                        --prev;
                        if (!std::isspace(static_cast<unsigned char>(*prev)))
                        {
                                break;
                        }
                        end = prev;
                }

                return std::string(begin, end);
        }

        std::optional<float2> ParseFloat2(const std::string& token)
        {
                const auto comma = token.find(',');
                if (comma == std::string::npos)
                {
                        return std::nullopt;
                }

                try
                {
                        float x = std::stof(token.substr(0, comma));
                        float y = std::stof(token.substr(comma + 1));
                        return float2{ x, y };
                }
                catch (...)
                {
                        return std::nullopt;
                }
        }

        std::optional<float3> ParseFloat3(const std::string& token)
        {
                const auto firstComma = token.find(',');
                if (firstComma == std::string::npos)
                {
                        return std::nullopt;
                }

                const auto secondComma = token.find(',', firstComma + 1);
                if (secondComma == std::string::npos)
                {
                        return std::nullopt;
                }

                try
                {
                        float x = std::stof(token.substr(0, firstComma));
                        float y = std::stof(token.substr(firstComma + 1, secondComma - firstComma - 1));
                        float z = std::stof(token.substr(secondComma + 1));
                        return float3{ x, y, z };
                }
                catch (...)
                {
                        return std::nullopt;
                }
        }

        std::optional<float2> ParseMarginVector2(const std::string& value)
        {
                if (value.find(',') == std::string::npos)
                {
                        return std::nullopt;
                }

                if (auto parsed = ParseFloat2(value))
                {
                        return parsed;
                }

                return std::nullopt;
        }

        std::optional<float3> ParseMarginVector3(const std::string& value)
        {
                if (value.find(',') == std::string::npos)
                {
                        return std::nullopt;
                }

                if (auto parsed = ParseFloat3(value))
                {
                        return parsed;
                }

                return std::nullopt;
        }

        std::optional<float2> SumOptional(const std::optional<float2>& lhs, const std::optional<float2>& rhs)
        {
                if (!lhs && !rhs)
                {
                        return std::nullopt;
                }

                float2 result{ 0.0f, 0.0f };
                if (lhs)
                {
                        result.x += lhs->x;
                        result.y += lhs->y;
                }
                if (rhs)
                {
                        result.x += rhs->x;
                        result.y += rhs->y;
                }
                return result;
        }

        std::optional<float3> SumOptional(const std::optional<float3>& lhs, const std::optional<float3>& rhs)
        {
                if (!lhs && !rhs)
                {
                        return std::nullopt;
                }

                float3 result{ 0.0f, 0.0f, 0.0f };
                if (lhs)
                {
                        result.x += lhs->x;
                        result.y += lhs->y;
                        result.z += lhs->z;
                }
                if (rhs)
                {
                        result.x += rhs->x;
                        result.y += rhs->y;
                        result.z += rhs->z;
                }
                return result;
        }

        std::vector<float2> ParsePoints(const std::string& raw)
        {
                std::vector<float2> points;
                std::stringstream stream(raw);
                std::string token;
                while (std::getline(stream, token, ';'))
                {
                        token = Trim(token);
                        if (token.empty())
                        {
                                continue;
                        }

                        const auto parsed = ParseFloat2(token);
                        if (parsed)
                        {
                                points.push_back(*parsed);
                        }
                }
                return points;
        }

        std::optional<Collision::ColliderKind> ParseKind(const std::string& raw)
        {
                if (raw == "circle" || raw == "Circle")
                {
                        return Collision::ColliderKind::Circle;
                }
                if (raw == "hull" || raw == "Hull")
                {
                        return Collision::ColliderKind::Hull;
                }
                if (raw == "compound" || raw == "Compound")
                {
                        return Collision::ColliderKind::Compound;
                }
                return std::nullopt;
        }

        void ApplyScalarOption(const std::string& key, const std::string& value, Collision::ColliderDefinition& outDef)
        {
                if (key == "margin")
                {
                        if (const auto vector3 = ParseMarginVector3(value))
                        {
                                outDef.margin = (*vector3).x;
                                outDef.margin3 = vector3;
                                outDef.margin2 = float2{ vector3->x, vector3->y };
                                return;
                        }

                        if (const auto vector2 = ParseMarginVector2(value))
                        {
                                outDef.margin = (*vector2).x;
                                outDef.margin2 = vector2;
                                outDef.margin3.reset();
                                return;
                        }

                        try
                        {
                                outDef.margin = std::stof(value);
                                outDef.margin2.reset();
                                outDef.margin3.reset();
                        }
                        catch (...)
                        {
                                std::ostringstream oss;
                                oss << kLogPrefix << "Ignoring invalid margin value '" << value
                                        << "'; expected scalar or comma-separated components.\n";
                                Logging::LogWarning(oss.str());
                        }
                        return;
                }

                try
                {
                        const float parsed = std::stof(value);
                        if (key == "scale")
                        {
                                const float sanitized = std::max(parsed, 0.0f);
#if defined(_DEBUG)
                                if (std::abs(sanitized - parsed) > 1e-4f)
                                {
                                        std::ostringstream oss;
                                        oss << kLogPrefix << "Clamped negative scale to " << sanitized << "\n";
                                        Logging::DebugTrace(oss.str());
                                }
#endif
                                outDef.scale = sanitized;
                        }
                }
                catch (...)
                {
#if defined(_DEBUG)
                        std::ostringstream oss;
                        oss << kLogPrefix << "Failed to parse scalar option '" << key << "' from value '" << value << "'\n";
                        Logging::DebugTrace(oss.str());
#endif
                }
        }

        bool ParseChild(const std::string& token, const std::string& colliderName, Collision::ColliderDefinition& outDef)
        {
                const auto colon = token.find(':');
                if (colon == std::string::npos)
                {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Ignoring malformed child token for collider '" << colliderName
                                << "': missing ':' in '" << token << "'\n";
                        Logging::LogWarning(oss.str());
                        return false;
                }

                const std::string typeToken = Trim(token.substr(0, colon));
                const std::string rest = Trim(token.substr(colon + 1));

                std::string shapeData = rest;
                std::string optionData;

                if (const auto optionPos = rest.find('|'); optionPos != std::string::npos)
                {
                        shapeData = Trim(rest.substr(0, optionPos));
                        optionData = rest.substr(optionPos + 1);
                }

                const auto atPos = shapeData.find('@');
                const std::string offsetToken = (atPos == std::string::npos) ? std::string{} : shapeData.substr(atPos + 1);
                if (atPos != std::string::npos)
                {
                        shapeData = Trim(shapeData.substr(0, atPos));
                }

                const auto kind = ParseKind(typeToken);
                if (!kind)
                {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Unknown child type for collider '" << colliderName << "': '" << typeToken
                                << "' in token '" << token << "'\n";
                        Logging::LogWarning(oss.str());
                        return false;
                }

                outDef.type = *kind;
                if (!offsetToken.empty())
                {
                        if (auto offset = ParseFloat2(offsetToken))
                        {
                                outDef.offset = *offset;
                        }
                }

                if (!optionData.empty())
                {
                        std::stringstream options(optionData);
                        std::string optionToken;
                        while (std::getline(options, optionToken, '|'))
                        {
                                optionToken = Trim(optionToken);
                                if (optionToken.empty())
                                {
                                        continue;
                                }

                                const auto eqPos = optionToken.find('=');
                                if (eqPos == std::string::npos)
                                {
                                        std::ostringstream oss;
                                        oss << kLogPrefix << "Ignoring malformed child option for collider '" << colliderName
                                                << "': '" << optionToken << "'\n";
                                        Logging::LogWarning(oss.str());
                                        continue;
                                }

                                const std::string key = Trim(optionToken.substr(0, eqPos));
                                const std::string value = Trim(optionToken.substr(eqPos + 1));
                                ApplyScalarOption(key, value, outDef);
                        }
                }

                if (outDef.type == Collision::ColliderKind::Circle)
                {
                        try
                        {
                                outDef.radius = std::stof(shapeData);
                        }
                        catch (...)
                        {
                                std::ostringstream oss;
                                oss << kLogPrefix << "Failed to parse circle radius for collider '" << colliderName
                                        << "' from '" << shapeData << "'\n";
                                Logging::LogWarning(oss.str());
                                return false;
                        }
                        return true;
                }

                if (outDef.type == Collision::ColliderKind::Hull)
                {
                        outDef.points = ParsePoints(shapeData);
                        if (outDef.points.empty())
                        {
                                std::ostringstream oss;
                                oss << kLogPrefix << "Hull collider '" << colliderName
                                        << "' contained no valid points in token '" << shapeData << "'\n";
                                Logging::LogWarning(oss.str());
                        }
                        return !outDef.points.empty();
                }

                return false;
        }
}

namespace Collision
{
        bool LoadColliderDefinitions(
                const std::string& path,
                const std::string& section,
                std::map<std::string, ColliderDefinition>& outDefinitions)
        {
                std::vector<Config::SectionLine> lines;
                if (!Config::ReadSectionLines(path, section, lines))
                {
                        std::ostringstream oss;
                        oss << kLogPrefix << "Failed to read collider definitions section '" << section
                                << "' from config: " << path << "\n";
                        Logging::LogWarning(oss.str());
                        return false;
                }

                std::string currentName;
                ColliderDefinition current{};
                bool hasActive = false;

                const auto flush = [&]()
                {
                        if (currentName.empty())
                        {
                                return;
                        }
                        outDefinitions[currentName] = current;
                        hasActive = false;
                };

                for (const auto& lineEntry : lines)
                {
                        const std::string key = lineEntry.key;
                        const std::string value = lineEntry.value;

                        if (key == "collider" || key == "name")
                        {
                                if (hasActive)
                                {
                                        flush();
                                }
                                current = ColliderDefinition{};
                                currentName = value;
                                hasActive = true;
                        }
                        else if (key == "shape" || key == "type")
                        {
                                if (auto kind = ParseKind(value))
                                {
                                        current.type = *kind;
                                }
                                else
                                {
                                        std::ostringstream oss;
                                        oss << kLogPrefix << "Unknown collider type for '"
                                                << (currentName.empty() ? std::string{"<unnamed>"} : currentName)
                                                << "': '" << value << "'\n";
                                        Logging::LogWarning(oss.str());
                                }
                        }
                        else if (key == "radius")
                        {
                                try
                                {
                                        current.radius = std::max(0.0f, std::stof(value));
                                }
                                catch (...)
                                {
                                        std::ostringstream oss;
                                        oss << kLogPrefix << "Failed to parse radius for collider '"
                                                << (currentName.empty() ? std::string{"<unnamed>"} : currentName)
                                                << "' from '" << value << "'\n";
                                        Logging::LogWarning(oss.str());
                                }
                        }
                        else if (key == "offset")
                        {
                                if (auto parsed = ParseFloat2(value))
                                {
                                        current.offset = *parsed;
                                }
                                else
                                {
                                        std::ostringstream oss;
                                        oss << kLogPrefix << "Failed to parse offset for collider '"
                                                << (currentName.empty() ? std::string{"<unnamed>"} : currentName)
                                                << "' from '" << value << "'\n";
                                        Logging::LogWarning(oss.str());
                                }
                        }
                        else if (key == "points")
                        {
                                current.points = ParsePoints(value);
                                if (current.points.empty())
                                {
                                        std::ostringstream oss;
                                        oss << kLogPrefix << "Collider '"
                                                << (currentName.empty() ? std::string{"<unnamed>"} : currentName)
                                                << "' contained no valid points in '" << value << "'\n";
                                        Logging::LogWarning(oss.str());
                                }
                        }
                        else if (key == "thickness")
                        {
                                try
                                {
                                        current.thickness = std::max(0.0f, std::stof(value));
                                }
                                catch (...)
                                {
                                        std::ostringstream oss;
                                        oss << kLogPrefix << "Failed to parse thickness for collider '"
                                                << (currentName.empty() ? std::string{"<unnamed>"} : currentName)
                                                << "' from '" << value << "'\n";
                                        Logging::LogWarning(oss.str());
                                }
                        }
                        else if (key == "scale")
                        {
                                ApplyScalarOption(key, value, current);
                        }
                        else if (key == "margin")
                        {
                                ApplyScalarOption(key, value, current);
                        }
                        else if (key == "children")
                        {
                                current.children.clear();
                                std::stringstream stream(value);
                                std::string token;
                                while (std::getline(stream, token, ';'))
                                {
                                        token = Trim(token);
                                        if (token.empty())
                                        {
                                                continue;
                                        }

                                        ColliderDefinition child{};
                                        const std::string childName = currentName.empty() ? std::string{"<unnamed>"} : currentName;
                                        if (ParseChild(token, childName, child))
                                        {
                                                current.children.push_back(child);
                                        }
                                        else
                                        {
                                                std::ostringstream oss;
                                                oss << kLogPrefix << "Skipping malformed child token for collider '" << childName
                                                        << "': '" << token << "'\n";
                                                Logging::LogWarning(oss.str());
                                        }
                                }
                        }
                }

                if (hasActive && !currentName.empty())
                {
                        flush();
                }

#if defined(_DEBUG)
                std::ostringstream oss;
                oss << kLogPrefix << "Loaded " << outDefinitions.size() << " collider definitions from " << path << "\n";
                Logging::DebugTrace(oss.str());
#endif

                return !outDefinitions.empty();
        }

}
