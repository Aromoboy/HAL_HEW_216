#pragma once

#include <box2d/box2d.h>
#include <vector>

#include "MeshBuffer.h"

class Physics2DWorld
{
public:
    explicit Physics2DWorld(const b2Vec2& gravity);
    ~Physics2DWorld();

    Physics2DWorld(const Physics2DWorld&) = delete;
    Physics2DWorld& operator=(const Physics2DWorld&) = delete;

    void Step(float dt);

    b2BodyId CreateBoxBody(const b2Vec2& position, float hx, float hy, float density, float friction);

    // Builds a polygon (or multi-polygon for concave silhouettes) body from the provided
    // mesh description. Vertices are projected onto the XY plane. If the mesh cannot be
    // decomposed, a convex hull will be generated instead. The provided filter is applied
    // to every fixture created for the body.
    b2BodyId CreateBodyFromMesh(
        const MeshBuffer::Description& desc,
        const b2BodyDef& bodyDef,
        float density,
        const b2Filter& filter = b2DefaultFilter());

    void DestroyBody(b2BodyId bodyId);

    // Returns true if the body touched any other fixture during the most recent world step.
    bool HadContact(b2BodyId bodyId) const;

private:
    void UpdateContactCache();

    b2WorldDef m_worldDef{}; // keep world configuration/allocator state alive for the world lifetime
    b2WorldId m_worldId{b2_nullWorldId};
    std::vector<b2BodyId> m_contactedBodies;
};

void TestBox2DIntegration();
