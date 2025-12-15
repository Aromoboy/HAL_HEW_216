#include "ColliderConfigLoader.h"

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
        bool LoadColliderDefinitions(const std::string& path, std::map<std::string, ColliderDefinition>& outDefinitions)
        {
                std::ifstream file(path);
                if (!file)
                {
                        Logging::LogWarning(std::string(kLogPrefix) + "Failed to open config: " + path + "\n");
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

                std::string line;
                while (std::getline(file, line))
                {
                        line = Trim(line);
                        if (line.empty() || line[0] == '#')
                        {
                                continue;
                        }

                        const auto eq = line.find('=');
                        if (eq == std::string::npos)
                        {
                                std::ostringstream oss;
                                oss << kLogPrefix << "Ignoring malformed line (missing '=') while parsing collider '"
                                        << (currentName.empty() ? std::string{"<unnamed>"} : currentName)
                                        << "': " << line << "\n";
                                Logging::LogWarning(oss.str());
                                continue;
                        }

                        const std::string key = Trim(line.substr(0, eq));
                        const std::string value = Trim(line.substr(eq + 1));

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

        std::shared_ptr<Collision::ICollider> BuildColliderFromDefinition(
                const ColliderDefinition& definition,
                const Transform& transform,
                Collision::CollisionLayer layer,
                float inheritedScale,
                float inheritedMargin,
                std::optional<float2> inheritedMargin2,
                std::optional<float3> inheritedMargin3)
        {
                const float effectiveScale = std::max(0.0f, definition.scale * inheritedScale);
                const bool hasOwnAxisMargin = definition.margin2 || definition.margin3;
                const auto effectiveMargin2 = SumOptional(definition.margin2, inheritedMargin2);
                const auto effectiveMargin3 = SumOptional(definition.margin3, inheritedMargin3);
                const bool hasAxisMargin = hasOwnAxisMargin || effectiveMargin2 || effectiveMargin3;
                const float effectiveMargin = (hasOwnAxisMargin ? 0.0f : definition.margin) + inheritedMargin;

                switch (definition.type)
                {
                case ColliderKind::Circle:
                {
                        const float2 axisMargin = effectiveMargin2
                                ? *effectiveMargin2
                                : (effectiveMargin3 ? float2{ effectiveMargin3->x, effectiveMargin3->y } : float2{ 0.0f, 0.0f });
                        const float circleMargin = effectiveMargin + (hasAxisMargin ? axisMargin.x : 0.0f);

                        if (hasAxisMargin && std::abs(axisMargin.y - axisMargin.x) > 1e-4f)
                        {
                                std::ostringstream oss;
                                oss << kLogPrefix
                                        << "Circle collider received non-uniform margin; using X component only ("
                                        << axisMargin.x << ", " << axisMargin.y << ").\n";
                                Logging::LogWarning(oss.str());
                        }

                        const float scaledRadius = std::max(0.0f, definition.radius * effectiveScale + circleMargin);
                        const float2 scaledOffset{ definition.offset.x * effectiveScale, definition.offset.y * effectiveScale };

#if defined(_DEBUG)
                        if (std::abs(effectiveScale - 1.0f) > 1e-4f || std::abs(circleMargin) > 1e-4f)
                        {
                                std::ostringstream oss;
                                oss << kLogPrefix << "Circle scale/margin applied: base radius=" << definition.radius
                                        << ", offset=(" << definition.offset.x << ", " << definition.offset.y << ")"
                                        << ", effectiveScale=" << effectiveScale << ", margin=" << circleMargin
                                        << ", final radius=" << scaledRadius
                                        << ", final offset=(" << scaledOffset.x << ", " << scaledOffset.y << ")\n";
                                Logging::DebugTrace(oss.str());
                        }
#endif

                        auto collider = std::make_shared<CircleCollider2D>(scaledRadius, scaledOffset, definition.thickness, layer);
                        collider->UpdateBounds(transform);
                        return collider;
                }
                case ColliderKind::Hull:
                {
                        if (definition.points.empty())
                        {
#if defined(_DEBUG)
                                std::ostringstream oss;
                                oss << kLogPrefix << "Hull collider definition contained no points; skipping creation.\n";
                                Logging::DebugTrace(oss.str());
#endif
                                return nullptr;
                        }

                        std::vector<float2> scaledPoints;
                        scaledPoints.reserve(definition.points.size());

                        for (const auto& p : definition.points)
                        {
                                float2 scaled{ p.x * effectiveScale, p.y * effectiveScale };
                                if (hasAxisMargin)
                                {
                                        const float2 axisMargin = effectiveMargin2
                                                ? *effectiveMargin2
                                                : float2{ effectiveMargin3 ? effectiveMargin3->x : 0.0f,
                                                          effectiveMargin3 ? effectiveMargin3->y : 0.0f };
                                        const float2 compositeMargin{ axisMargin.x + effectiveMargin, axisMargin.y + effectiveMargin };
                                        scaled.x += (scaled.x >= 0.0f ? compositeMargin.x : -compositeMargin.x);
                                        scaled.y += (scaled.y >= 0.0f ? compositeMargin.y : -compositeMargin.y);
                                }
                                else if (std::abs(effectiveMargin) > 1e-6f)
                                {
                                        const float lenSq = (scaled.x * scaled.x) + (scaled.y * scaled.y);
                                        if (lenSq > 1e-8f)
                                        {
                                                const float invLen = 1.0f / std::sqrt(lenSq);
                                                scaled.x += scaled.x * invLen * effectiveMargin;
                                                scaled.y += scaled.y * invLen * effectiveMargin;
                                        }
                                        else
                                        {
                                                scaled.x += effectiveMargin;
                                        }
                                }
                                scaledPoints.push_back(scaled);
                        }

                        const float2 scaledOffset{ definition.offset.x * effectiveScale, definition.offset.y * effectiveScale };

#if defined(_DEBUG)
                        if (std::abs(effectiveScale - 1.0f) > 1e-4f || std::abs(effectiveMargin) > 1e-4f)
                        {
                                std::ostringstream oss;
                                oss << kLogPrefix << "Hull scale/margin applied: points=" << definition.points.size()
                                        << ", effectiveScale=" << effectiveScale << ", margin=" << effectiveMargin
                                        << ", offset=(" << definition.offset.x << ", " << definition.offset.y << ")"
                                        << ", final offset=(" << scaledOffset.x << ", " << scaledOffset.y << ")\n";
                                Logging::DebugTrace(oss.str());
                        }
#endif

                        auto collider = std::make_shared<ConvexHullCollider2D>(scaledPoints, scaledOffset, definition.thickness, layer);
                        collider->UpdateBounds(transform);
                        return collider;
                }
                case ColliderKind::Compound:
                {
                        std::vector<std::shared_ptr<Collision::ICollider>> children;
                        children.reserve(definition.children.size());
                        for (const auto& childDef : definition.children)
                        {
                                auto child = BuildColliderFromDefinition(
                                        childDef,
                                        transform,
                                        layer,
                                        effectiveScale,
                                        effectiveMargin,
                                        effectiveMargin2,
                                        effectiveMargin3);
                                if (child)
                                {
                                        children.push_back(child);
                                }
                        }
                        auto collider = std::make_shared<CompoundCollider2D>(children, definition.thickness, layer);
                        collider->UpdateBounds(transform);
                        return collider;
                }
                default:
                        break;
                }

                return nullptr;
        }
}

