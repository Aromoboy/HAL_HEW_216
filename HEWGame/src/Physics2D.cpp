#include "Physics2D.h"

#include <cstdint>
#include <cstdio>
#include <box2d/math_functions.h>

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

void Physics2DWorld::DestroyBody(b2BodyId bodyId)
{
    if (b2Body_IsValid(bodyId))
    {
        b2DestroyBody(bodyId);
    }
}

void TestBox2DIntegration()
{
    Physics2DWorld world(b2Vec2(0.0f, -9.8f));

    b2BodyId ground = world.CreateBoxBody(b2Vec2(0.0f, -10.0f), 50.0f, 10.0f, 0.0f, 0.3f);
    (void)ground;

    b2BodyId dynamicBox = world.CreateBoxBody(b2Vec2(0.0f, 4.0f), 1.0f, 1.0f, 1.0f, 0.3f);
    if (!b2Body_IsValid(dynamicBox))
    {
        std::printf("Failed to create dynamic box body\n");
        return;
    }

    const float timeStep = 1.0f / 60.0f;
    for (int32_t i = 0; i < 60; ++i)
    {
        world.Step(timeStep);
        const b2Vec2 position = b2Body_GetPosition(dynamicBox);
        const float angle = b2Rot_GetAngle(b2Body_GetRotation(dynamicBox));
        std::printf("step %d: position=(%.2f, %.2f) angle=%.2f\n", i, position.x, position.y, angle);
    }
}
