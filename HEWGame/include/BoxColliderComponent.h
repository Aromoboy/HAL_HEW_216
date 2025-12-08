#pragma once

#include <vector>

#include <box2d/box2d.h>

namespace Physics
{
    // Simple collider component that pre-builds Box2D shapes for an entity.
    struct BoxColliderComponent
    {
        b2ShapeDef shapeDef{ b2DefaultShapeDef() };
        std::vector<b2Polygon> polygons;
        std::vector<b2Circle> circles;

        void Clear()
        {
            polygons.clear();
            circles.clear();
        }

        bool Empty() const
        {
            return polygons.empty() && circles.empty();
        }

        void AddBox(float halfWidth, float halfHeight, b2Vec2 center = b2Vec2{ 0.0f, 0.0f })
        {
            polygons.push_back(b2MakeOffsetBox(halfWidth, halfHeight, center, b2Rot_identity));
        }

        void AddPolygon(const std::vector<b2Vec2>& vertices)
        {
            if (vertices.size() < 3)
            {
                return;
            }

            const b2Hull hull = b2ComputeHull(vertices.data(), static_cast<int>(vertices.size()));
            if (!b2ValidateHull(&hull))
            {
                return;
            }

            polygons.push_back(b2MakePolygon(&hull, b2_defaultPolygonRadius));
        }

        void AddCircle(float radius)
        {
            if (radius <= 0.0f)
            {
                return;
            }

            circles.push_back(b2Circle{ b2Vec2{ 0.0f, 0.0f }, radius });
        }
    };
}

