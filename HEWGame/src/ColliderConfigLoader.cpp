#include "ColliderConfigLoader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace
{
        constexpr const char* kLogPrefix = "[ColliderConfig] ";

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

        bool ParseChild(const std::string& token, Collision::ColliderDefinition& outDef)
        {
                const auto colon = token.find(':');
                if (colon == std::string::npos)
                {
                        return false;
                }

                const std::string typeToken = Trim(token.substr(0, colon));
                const std::string rest = Trim(token.substr(colon + 1));
                const auto atPos = rest.find('@');
                const std::string shapeData = (atPos == std::string::npos) ? rest : rest.substr(0, atPos);
                const std::string offsetToken = (atPos == std::string::npos) ? std::string{} : rest.substr(atPos + 1);

                const auto kind = ParseKind(typeToken);
                if (!kind)
                {
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

                if (outDef.type == Collision::ColliderKind::Circle)
                {
                        try
                        {
                                outDef.radius = std::stof(shapeData);
                        }
                        catch (...)
                        {
                                return false;
                        }
                        return true;
                }

                if (outDef.type == Collision::ColliderKind::Hull)
                {
                        outDef.points = ParsePoints(shapeData);
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
#if defined(_DEBUG)
                        DebugTrace(std::string(kLogPrefix) + "Failed to open config: " + path + "\n");
#endif
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
                        }
                        else if (key == "radius")
                        {
                                try
                                {
                                        current.radius = std::max(0.0f, std::stof(value));
                                }
                                catch (...)
                                {
                                }
                        }
                        else if (key == "offset")
                        {
                                if (auto parsed = ParseFloat2(value))
                                {
                                        current.offset = *parsed;
                                }
                        }
                        else if (key == "points")
                        {
                                current.points = ParsePoints(value);
                        }
                        else if (key == "thickness")
                        {
                                try
                                {
                                        current.thickness = std::max(0.0f, std::stof(value));
                                }
                                catch (...)
                                {
                                }
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
                                        if (ParseChild(token, child))
                                        {
                                                current.children.push_back(child);
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
                DebugTrace(oss.str());
#endif

                return !outDefinitions.empty();
        }

        std::shared_ptr<Collision::ICollider> BuildColliderFromDefinition(
                const ColliderDefinition& definition,
                const Transform& transform,
                Collision::CollisionLayer layer)
        {
                switch (definition.type)
                {
                case ColliderKind::Circle:
                {
                        auto collider = std::make_shared<CircleCollider2D>(definition.radius, definition.offset, definition.thickness, layer);
                        collider->UpdateBounds(transform);
                        return collider;
                }
                case ColliderKind::Hull:
                {
                        auto collider = std::make_shared<ConvexHullCollider2D>(definition.points, definition.offset, definition.thickness, layer);
                        collider->UpdateBounds(transform);
                        return collider;
                }
                case ColliderKind::Compound:
                {
                        std::vector<std::shared_ptr<Collision::ICollider>> children;
                        children.reserve(definition.children.size());
                        for (const auto& childDef : definition.children)
                        {
                                auto child = BuildColliderFromDefinition(childDef, transform, layer);
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

