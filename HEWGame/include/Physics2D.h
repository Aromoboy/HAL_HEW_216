#pragma once

#include <box2d/box2d.h>

class Physics2DWorld
{
public:
    explicit Physics2DWorld(const b2Vec2& gravity);

    void Step(float dt);

    b2Body* CreateBoxBody(const b2Vec2& position, float hx, float hy, float density, float friction);

    void DestroyBody(b2Body* body);

private:
    b2World m_world;
};

void TestBox2DIntegration();
