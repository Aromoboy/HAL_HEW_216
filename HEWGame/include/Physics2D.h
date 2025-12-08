#pragma once

#include <box2d/box2d.h>

class Physics2DWorld
{
public:
    explicit Physics2DWorld(const b2Vec2& gravity);
    ~Physics2DWorld();

    Physics2DWorld(const Physics2DWorld&) = delete;
    Physics2DWorld& operator=(const Physics2DWorld&) = delete;

    void Step(float dt);

    b2BodyId CreateBoxBody(const b2Vec2& position, float hx, float hy, float density, float friction);

    void DestroyBody(b2BodyId bodyId);

private:
    b2WorldDef m_worldDef{}; // keep world configuration/allocator state alive for the world lifetime
    b2WorldId m_worldId{b2_nullWorldId};
};

void TestBox2DIntegration();
