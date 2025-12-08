#include "Physics2D.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <vector>

#include <box2d/collision.h>
#include <box2d/math_functions.h>

#include "Collision/ConvexHullBuilder.h"

Physics2DWorld::Physics2DWorld(const b2Vec2& gravity)
{
    m_worldDef = b2DefaultWorldDef();
    m_worldDef.gravity = gravity;
    m_worldId = b2CreateWorld(&m_worldDef);
}

Physics2DWorld::~Physics2DWorld()
{
    if (b2World_IsValid(m_worldId))
    {
        b2DestroyWorld(m_worldId);
        m_worldId = b2_nullWorldId;
    }
}

void Physics2DWorld::Step(float dt)
{
    if (b2World_IsValid(m_worldId))
    {
        constexpr int subStepCount = 4;
        b2World_Step(m_worldId, dt, subStepCount);
        UpdateContactCache();
    }
}

b2BodyId Physics2DWorld::CreateBoxBody(const b2Vec2& position, float hx, float hy, float density, float friction)
{
    if (!b2World_IsValid(m_worldId))
    {
        return b2_nullBodyId;
    }

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.position = position;
    bodyDef.type = density > 0.0f ? b2_dynamicBody : b2_staticBody;

    b2BodyId bodyId = b2CreateBody(m_worldId, &bodyDef);
    if (!b2Body_IsValid(bodyId))
    {
        return b2_nullBodyId;
    }

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = density;
    shapeDef.material.friction = friction;

    const b2Polygon box = b2MakeBox(hx, hy);
    b2CreatePolygonShape(bodyId, &shapeDef, &box);

    return bodyId;
}

namespace
{
    std::vector<std::vector<b2Vec2>> BuildPolygonsFromMesh(const MeshBuffer::Description& desc)
    {
        std::vector<std::vector<b2Vec2>> polygons;
        Collision::Mesh2DExtraction extraction = Collision::ExtractProjectedPolygons(desc);
        if (!extraction.polygons.empty())
        {
            polygons.reserve(extraction.polygons.size());
            for (const auto& polygon : extraction.polygons)
            {
                if (polygon.size() < 3)
                {
                    continue;
                }

                std::vector<b2Vec2> converted;
                converted.reserve(polygon.size());
                for (const float2& point : polygon)
                {
                    converted.push_back(b2Vec2{ point.x, point.y });
                }

                polygons.push_back(std::move(converted));
            }
            return polygons;
        }

        const Collision::HullBuildResult hullResult = Collision::BuildConvexHullFromMesh(desc);
        if (!hullResult.hullPointsCentered.empty())
        {
            std::vector<b2Vec2> hull;
            hull.reserve(hullResult.hullPointsCentered.size());
            for (const float2& point : hullResult.hullPointsCentered)
            {
                hull.push_back(b2Vec2{ point.x + hullResult.centroidOffset.x, point.y + hullResult.centroidOffset.y });
            }
            polygons.push_back(std::move(hull));
        }

        return polygons;
    }
}

b2BodyId Physics2DWorld::CreateBodyFromMesh(
    const MeshBuffer::Description& desc,
    const b2BodyDef& bodyDef,
    float density,
    const b2Filter& filter)
{
    if (!b2World_IsValid(m_worldId))
    {
        return b2_nullBodyId;
    }

    const std::vector<std::vector<b2Vec2>> polygons = BuildPolygonsFromMesh(desc);
    if (polygons.empty())
    {
        return b2_nullBodyId;
    }

    b2BodyId bodyId = b2CreateBody(m_worldId, &bodyDef);
    if (!b2Body_IsValid(bodyId))
    {
        return b2_nullBodyId;
    }

    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.filter = filter;
    shapeDef.density = density;

    for (const auto& polygon : polygons)
    {
        const b2Hull hull = b2ComputeHull(polygon.data(), static_cast<int>(polygon.size()));
        if (!b2ValidateHull(&hull))
        {
            continue;
        }

        const b2Polygon shape = b2MakePolygon(&hull, b2_polygonRadius);
        b2CreatePolygonShape(bodyId, &shapeDef, &shape);
    }

    return bodyId;
}

void Physics2DWorld::DestroyBody(b2BodyId bodyId)
{
    if (b2Body_IsValid(bodyId))
    {
        b2DestroyBody(bodyId);
    }
}

void Physics2DWorld::UpdateContactCache()
{
    m_contactedBodies.clear();
    if (!b2World_IsValid(m_worldId))
    {
        return;
    }

    const b2ContactEvents events = b2World_GetContactEvents(m_worldId);
    m_contactedBodies.reserve(static_cast<size_t>(events.beginCount));
    for (int i = 0; i < events.beginCount; ++i)
    {
        const b2ContactBeginTouchEvent& evt = events.beginEvents[i];
        if (b2Body_IsValid(evt.bodyIdA))
        {
            m_contactedBodies.push_back(evt.bodyIdA);
        }
        if (b2Body_IsValid(evt.bodyIdB))
        {
            m_contactedBodies.push_back(evt.bodyIdB);
        }
    }

    if (events.beginEvents)
    {
        b2Free(events.beginEvents);
    }
    if (events.endEvents)
    {
        b2Free(events.endEvents);
    }
    if (events.hitEvents)
    {
        b2Free(events.hitEvents);
    }

    std::sort(m_contactedBodies.begin(), m_contactedBodies.end(), [](b2BodyId lhs, b2BodyId rhs)
        {
            return lhs.index < rhs.index;
        });
    m_contactedBodies.erase(
        std::unique(m_contactedBodies.begin(), m_contactedBodies.end(), [](b2BodyId lhs, b2BodyId rhs)
            {
                return lhs.index == rhs.index && lhs.revision == rhs.revision;
            }),
        m_contactedBodies.end());
}

bool Physics2DWorld::HadContact(b2BodyId bodyId) const
{
    const auto it = std::lower_bound(
        m_contactedBodies.begin(),
        m_contactedBodies.end(),
        bodyId,
        [](b2BodyId lhs, b2BodyId rhs)
        {
            return lhs.index < rhs.index;
        });

    return it != m_contactedBodies.end() && it->index == bodyId.index && it->revision == bodyId.revision;
}

void TestBox2DIntegration()
{
    Physics2DWorld world(b2Vec2{0.0f, -9.8f});

    // Concave mesh represented as two rectangles (L-shape) to validate multi-fixture build
    struct Vertex
    {
        float x, y, z;
    };

    const Vertex vertices[] = {
        { -1.0f, 0.0f, 0.0f }, { -1.0f, 2.0f, 0.0f }, { -0.5f, 2.0f, 0.0f },
        { -0.5f, 0.5f, 0.0f }, { 1.0f, 0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f },
    };

    const uint16_t indices[] = { 0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 5 };

    MeshBuffer::Description concaveDesc{};
    concaveDesc.pVtx = vertices;
    concaveDesc.vtxSize = sizeof(Vertex);
    concaveDesc.vtxCount = static_cast<UINT>(std::size(vertices));
    concaveDesc.isWrite = false;
    concaveDesc.pIdx = indices;
    concaveDesc.idxSize = sizeof(uint16_t);
    concaveDesc.idxCount = static_cast<UINT>(std::size(indices));
    concaveDesc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    b2BodyDef concaveBodyDef = b2DefaultBodyDef();
    concaveBodyDef.position = b2Vec2{ 0.0f, 4.0f };
    concaveBodyDef.type = b2_dynamicBody;

    const b2Filter defaultFilter = b2DefaultFilter();
    b2BodyId concaveBody = world.CreateBodyFromMesh(concaveDesc, concaveBodyDef, 1.0f, defaultFilter);
    if (!b2Body_IsValid(concaveBody))
    {
        std::printf("Failed to create concave mesh body\n");
        return;
    }

    b2BodyId ground = world.CreateBoxBody(b2Vec2{0.0f, -10.0f}, 50.0f, 10.0f, 0.0f, 0.3f);
    (void)ground;

    const float timeStep = 1.0f / 60.0f;
    for (int32_t i = 0; i < 60; ++i)
    {
        world.Step(timeStep);
        const b2Vec2 position = b2Body_GetPosition(concaveBody);
        const float angle = b2Rot_GetAngle(b2Body_GetRotation(concaveBody));
        const bool touched = world.HadContact(concaveBody);
        std::printf(
            "step %d: position=(%.2f, %.2f) angle=%.2f contact=%s\n",
            i,
            position.x,
            position.y,
            angle,
            touched ? "true" : "false");
    }
}
