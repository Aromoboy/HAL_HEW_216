#pragma once

#include <vector>

#include <box2d/box2d.h>

namespace Physics
{
    // Simple collider component that pre-builds Box2D shapes for an entity and stores created bodies/fixtures.
    struct BoxColliderComponent
    {
        b2ShapeDef shapeDef{ b2DefaultShapeDef() };
        std::vector<b2Polygon> polygons;
        std::vector<b2Circle> circles;
        b2BodyId bodyId{ b2_nullBodyId };
        std::vector<b2ShapeId> fixtures;

        void Clear()
        {
            polygons.clear();
            circles.clear();
            fixtures.clear();
            bodyId = b2_nullBodyId;
            shapeDef = b2DefaultShapeDef();
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

            // Box2D v3 doesn't expose b2_defaultPolygonRadius; use explicit radius instead
            polygons.push_back(b2MakePolygon(&hull, 0.0f));
        }

        void AddCircle(float radius)
        {
            if (radius <= 0.0f)
            {
                return;
            }

            circles.push_back(b2Circle{ b2Vec2{ 0.0f, 0.0f }, radius });
        }

        void Destroy(b2WorldId worldId)
        {
            if (!b2World_IsValid(worldId))
            {
                return;
            }

            for (const b2ShapeId fixtureId : fixtures)
            {
                if (b2Shape_IsValid(fixtureId))
                {
                    b2DestroyShape(fixtureId, true);
                }
            }
            fixtures.clear();

            if (b2Body_IsValid(bodyId))
            {
                b2DestroyBody(bodyId);
            }

            bodyId = b2_nullBodyId;
        }

        void CreateFixtures(b2BodyId body, const b2ShapeDef& baseShapeDef)
        {
            bodyId = body;
            fixtures.clear();

            b2ShapeDef shapeDef = baseShapeDef;
            if (!shapeDef.userData)
            {
                shapeDef.userData = b2Body_GetUserData(bodyId);
            }
            for (const auto& polygon : polygons)
            {
                const b2ShapeId shapeId = b2CreatePolygonShape(bodyId, &shapeDef, &polygon);
                if (b2Shape_IsValid(shapeId))
                {
                    fixtures.push_back(shapeId);
                }
            }

            for (const auto& circle : circles)
            {
                const b2ShapeId shapeId = b2CreateCircleShape(bodyId, &shapeDef, &circle);
                if (b2Shape_IsValid(shapeId))
                {
                    fixtures.push_back(shapeId);
                }
            }
        }
    };
}

