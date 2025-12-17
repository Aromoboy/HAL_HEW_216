#pragma once

#include <optional>
#include <vector>
#include <string>

#include <box2d/box2d.h>

namespace Physics
{
    enum class ColliderShapeType
    {
        Box,
        Polygon,
        Circle,
    };

    struct ColliderShapeDescriptor
    {
        ColliderShapeType type{ ColliderShapeType::Polygon };
        std::vector<b2Vec2> vertices{};
        float radius{ 0.0f };
    };

    // Simple collider component that describes shapes and materials for an entity.
    struct BoxColliderComponent
    {
        // Identifier used to resolve shared collider definitions from the registry.
        std::string colliderId{};
        // Optional local offset applied to all shapes after lookup.
        b2Vec2 colliderOffset{ 0.0f, 0.0f };
        b2ShapeDef shapeDef{ b2DefaultShapeDef() };
        std::vector<ColliderShapeDescriptor> shapes;
        int createdFixtureCount{ 0 };
        int fixtureBuildFailures{ 0 };
        std::optional<std::size_t> lastFailureConfigHash{};

        void Clear()
        {
            shapes.clear();
            createdFixtureCount = 0;
            shapeDef = b2DefaultShapeDef();
            fixtureBuildFailures = 0;
            lastFailureConfigHash.reset();
            colliderId.clear();
            colliderOffset = b2Vec2{ 0.0f, 0.0f };
        }

        bool Empty() const
        {
            return shapes.empty();
        }

        void AddBox(float halfWidth, float halfHeight, b2Vec2 center = b2Vec2{ 0.0f, 0.0f })
        {
            if (halfWidth <= 0.0f || halfHeight <= 0.0f)
            {
                return;
            }

            ColliderShapeDescriptor shape{};
            shape.type = ColliderShapeType::Box;
            shape.vertices =
            {
                b2Vec2{ center.x - halfWidth, center.y - halfHeight },
                b2Vec2{ center.x + halfWidth, center.y - halfHeight },
                b2Vec2{ center.x + halfWidth, center.y + halfHeight },
                b2Vec2{ center.x - halfWidth, center.y + halfHeight },
            };

            shapes.push_back(std::move(shape));
        }

        void AddPolygon(const std::vector<b2Vec2>& vertices)
        {
            if (vertices.size() < 3)
            {
                return;
            }

            ColliderShapeDescriptor shape{};
            shape.type = ColliderShapeType::Polygon;
            shape.vertices = vertices;
            shapes.push_back(std::move(shape));
        }

        void AddCircle(float radius)
        {
            if (radius <= 0.0f)
            {
                return;
            }

            ColliderShapeDescriptor shape{};
            shape.type = ColliderShapeType::Circle;
            shape.radius = radius;
            shapes.push_back(std::move(shape));
        }
    };
}

