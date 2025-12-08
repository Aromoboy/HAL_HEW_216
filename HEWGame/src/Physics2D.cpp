#include "Physics2D.h"

#include <cstdio>

Physics2DWorld::Physics2DWorld(const b2Vec2& gravity)
    : m_world(gravity)
{
}

void Physics2DWorld::Step(float dt)
{
    const int32 velocityIterations = 8;
    const int32 positionIterations = 3;
    m_world.Step(dt, velocityIterations, positionIterations);
}

b2Body* Physics2DWorld::CreateBoxBody(const b2Vec2& position, float hx, float hy, float density, float friction)
{
    b2BodyDef bodyDef{};
    bodyDef.position = position;
    bodyDef.type = density > 0.0f ? b2_dynamicBody : b2_staticBody;

    b2Body* body = m_world.CreateBody(&bodyDef);

    b2PolygonShape boxShape{};
    boxShape.SetAsBox(hx, hy);

    b2FixtureDef fixtureDef{};
    fixtureDef.shape = &boxShape;
    fixtureDef.density = density;
    fixtureDef.friction = friction;

    body->CreateFixture(&fixtureDef);
    return body;
}

void Physics2DWorld::DestroyBody(b2Body* body)
{
    if (body != nullptr)
    {
        m_world.DestroyBody(body);
    }
}

void TestBox2DIntegration()
{
    Physics2DWorld world(b2Vec2(0.0f, -9.8f));

    b2Body* ground = world.CreateBoxBody(b2Vec2(0.0f, -10.0f), 50.0f, 10.0f, 0.0f, 0.3f);
    (void)ground;

    b2Body* dynamicBox = world.CreateBoxBody(b2Vec2(0.0f, 4.0f), 1.0f, 1.0f, 1.0f, 0.3f);

    const float timeStep = 1.0f / 60.0f;
    for (int32 i = 0; i < 60; ++i)
    {
        world.Step(timeStep);
        const b2Vec2 position = dynamicBox->GetPosition();
        const float angle = dynamicBox->GetAngle();
        std::printf("step %d: position=(%.2f, %.2f) angle=%.2f\n", i, position.x, position.y, angle);
    }
}
