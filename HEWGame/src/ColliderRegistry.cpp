#include "ColliderRegistry.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <optional>
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

    constexpr const char* kLogPrefix = "[ColliderRegistry] ";

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

    std::optional<float> ParseFloat(const std::string& token)
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

    std::optional<b2Vec2> ParseOffset(const std::string& xToken, const std::string& yToken)
    {
        const auto x = ParseFloat(xToken);
        const auto y = ParseFloat(yToken);
        if (!x || !y)
        {
            return std::nullopt;
        }

        return b2Vec2{ *x, *y };
    }

    enum class ShapeKind
    {
        Circle,
        Capsule,
        Box,
    };

    std::optional<ShapeKind> ParseShapeKind(const std::string& token)
    {
        if (token == "circle") return ShapeKind::Circle;
        if (token == "capsule") return ShapeKind::Capsule;
        if (token == "box") return ShapeKind::Box;
        return std::nullopt;
    }

    std::vector<b2Vec2> BuildCapsuleVertices(float radius, float height)
    {
        std::vector<b2Vec2> vertices;
        if (!(radius > 0.0f) || !(height > 0.0f))
        {
            return vertices;
        }

        const float halfWidth = radius;
        const float halfHeight = height * 0.5f;
        const bool horizontal = halfWidth >= halfHeight;
        const float spine = (horizontal ? halfWidth : halfHeight) - radius;
        if (!(radius > 0.0f))
        {
            return vertices;
        }

        constexpr int kArcSegments = 6;
        const float startRight = -B2_PI * 0.5f;
        const float endRight = B2_PI * 0.5f;
        const float startLeft = B2_PI * 0.5f;
        const float endLeft = B2_PI * 1.5f;

        const float offsetX = horizontal ? spine : 0.0f;
        const float offsetY = horizontal ? 0.0f : spine;

        auto appendArc = [&](float cx, float cy, float start, float end, bool skipFirst)
        {
            const float step = (end - start) / static_cast<float>(kArcSegments);
            for (int i = 0; i <= kArcSegments; ++i)
            {
                if (skipFirst && i == 0)
                {
                    continue;
                }

                const float angle = start + (step * static_cast<float>(i));
                vertices.push_back(b2Vec2{ cx + (radius * std::cos(angle)), cy + (radius * std::sin(angle)) });
            }
        };

        appendArc(offsetX, offsetY, startRight, endRight, false);
        appendArc(-offsetX, -offsetY, startLeft, endLeft, true);

        return vertices;
    }

    void ApplyOffset(std::vector<b2Vec2>& vertices, const b2Vec2& offset)
    {
        if (vertices.empty())
        {
            return;
        }

        for (auto& vertex : vertices)
        {
            vertex.x += offset.x;
            vertex.y += offset.y;
        }
    }
}

namespace Physics
{
    ColliderRegistry& ColliderRegistry::Instance()
    {
        static ColliderRegistry instance;
        return instance;
    }

    std::string ColliderRegistry::NormalizeKey(const std::string& id)
    {
        std::string normalized = id;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
        return normalized;
    }

    bool ColliderRegistry::LoadFromFile(const std::string& path)
    {
        std::ifstream file(path);
        if (!file)
        {
            std::ostringstream oss;
            oss << kLogPrefix << "Failed to open collider registry at '" << path << "'\n";
            DebugTrace(oss.str());
            return false;
        }

        m_definitions.clear();

        std::string line;
        std::string currentName;
        std::optional<ShapeKind> currentKind;
        float radius = 0.0f;
        float height = 0.0f;
        float width = 0.0f;
        b2Vec2 offset{ 0.0f, 0.0f };
        b2ShapeDef shapeDef = b2DefaultShapeDef();

        const auto flush = [&]()
        {
            if (currentName.empty() || !currentKind)
            {
                return;
            }

            ColliderRegistryDefinition def{};
            def.shapeDef = shapeDef;

            switch (*currentKind)
            {
            case ShapeKind::Circle:
            {
                ColliderShapeDescriptor shape{};
                shape.type = ColliderShapeType::Circle;
                shape.radius = radius;
                shape.vertices.push_back(offset);
                def.shapes.push_back(std::move(shape));
                break;
            }
            case ShapeKind::Capsule:
            {
                auto vertices = BuildCapsuleVertices(radius, height);
                ApplyOffset(vertices, offset);
                if (!vertices.empty())
                {
                    ColliderShapeDescriptor shape{};
                    shape.type = ColliderShapeType::Polygon;
                    shape.vertices = std::move(vertices);
                    def.shapes.push_back(std::move(shape));
                }
                break;
            }
            case ShapeKind::Box:
            {
                if (width > 0.0f && height > 0.0f)
                {
                    ColliderShapeDescriptor shape{};
                    shape.type = ColliderShapeType::Box;
                    shape.vertices =
                    {
                        b2Vec2{ offset.x - width * 0.5f, offset.y - height * 0.5f },
                        b2Vec2{ offset.x + width * 0.5f, offset.y - height * 0.5f },
                        b2Vec2{ offset.x + width * 0.5f, offset.y + height * 0.5f },
                        b2Vec2{ offset.x - width * 0.5f, offset.y + height * 0.5f },
                    };
                    def.shapes.push_back(std::move(shape));
                }
                break;
            }
            }

            if (def.shapes.empty())
            {
                std::ostringstream oss;
                oss << kLogPrefix << "Collider '" << currentName << "' has no valid shapes; skipping.\n";
                DebugTrace(oss.str());
            }
            else
            {
                const std::string normalized = NormalizeKey(currentName);
                m_definitions[normalized] = def;
            }
        };

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
                oss << kLogPrefix << "Ignoring malformed line in registry: '" << line << "'\n";
                DebugTrace(oss.str());
                continue;
            }

            const std::string key = Trim(line.substr(0, eq));
            const std::string value = Trim(line.substr(eq + 1));

            if (key == "collider" || key == "name")
            {
                flush();
                currentName = value;
                currentKind.reset();
                radius = 0.0f;
                height = 0.0f;
                width = 0.0f;
                offset = b2Vec2{ 0.0f, 0.0f };
                shapeDef = b2DefaultShapeDef();
                continue;
            }

            if (key == "shape")
            {
                currentKind = ParseShapeKind(value);
                continue;
            }

            if (key == "radius")
            {
                if (auto parsed = ParseFloat(value)) radius = *parsed;
                continue;
            }

            if (key == "height")
            {
                if (auto parsed = ParseFloat(value)) height = *parsed;
                continue;
            }

            if (key == "width")
            {
                if (auto parsed = ParseFloat(value)) width = *parsed;
                continue;
            }

            if (key == "offsetX")
            {
                if (auto parsed = ParseFloat(value)) offset.x = *parsed;
                continue;
            }

            if (key == "offsetY")
            {
                if (auto parsed = ParseFloat(value)) offset.y = *parsed;
                continue;
            }

            if (key == "friction")
            {
                if (auto parsed = ParseFloat(value))
                {
                    shapeDef.material.friction = std::max(0.0f, *parsed);
                }
                continue;
            }

            if (key == "restitution")
            {
                if (auto parsed = ParseFloat(value))
                {
                    shapeDef.material.restitution = std::clamp(*parsed, 0.0f, 1.0f);
                }
                continue;
            }

            if (key == "density")
            {
                if (auto parsed = ParseFloat(value))
                {
                    shapeDef.density = std::max(0.0f, *parsed);
                }
                continue;
            }
        }

        flush();

        if (m_definitions.empty())
        {
            std::ostringstream oss;
            oss << kLogPrefix << "No collider definitions loaded from '" << path << "'\n";
            DebugTrace(oss.str());
        }

        return !m_definitions.empty();
    }

    std::optional<ColliderRegistryDefinition> ColliderRegistry::Find(const std::string& id) const
    {
        const std::string normalized = NormalizeKey(id);
        auto it = m_definitions.find(normalized);
        if (it != m_definitions.end())
        {
            return it->second;
        }
        return std::nullopt;
    }
}

