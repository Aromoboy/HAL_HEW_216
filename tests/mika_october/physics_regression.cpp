#ifdef NDEBUG
#undef NDEBUG
#endif
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

#include "Entities/PhysicalEntity.h"
#include "Systems/Physics/Box2DContactListener.h"
#include "Systems/Physics/Box2DPhysicsSystem.h"
#include "Systems/Physics/PhysicsMachine.h"

namespace
{
    constexpr float kStep = 1.0f / 60.0f;

    std::unique_ptr<Entities::PhysicalEntity> MakeBody(
        Systems::PhysicsMachine& physics, float x, float y, b2BodyType type, float angle = 0.0f)
    {
        Transform transform{};
        transform.position = { x, y, 0.0f };
        transform.rotation.z = angle;
        transform.scale = { 1.0f, 1.0f, 1.0f };
        Physics::Box2DShapeConfig shape;
        shape.type = Physics::Box2DShapeConfig::Type::Box;
        shape.bodyType = type;
        auto entity = std::make_unique<Entities::PhysicalEntity>(transform, shape);
        Physics::BoxColliderComponent collider;
        collider.AddBox(0.5f, 0.5f);
        collider.shapeDef.enableContactEvents = true;
        entity->SetBoxColliderComponent(std::move(collider));
        entity->AttachPhysicsMachine(&physics);
        assert(entity->HasBox2DBody());
        return entity;
    }

    void CheckContactCapture()
    {
        Systems::PhysicsTuningParameters tuning;
        tuning.gravity = 0.0f;
        Systems::PhysicsMachine physics(tuning);
        auto listener = std::make_shared<Systems::Box2DContactListener>(physics);
        physics.SetContactListener(listener);
        auto floor = MakeBody(physics, 0.0f, 0.0f, b2_staticBody);
        auto fruit = MakeBody(physics, 0.0f, 0.9f, b2_dynamicBody);
        fruit->SetLinearVelocity({ 0.0f, -6.0f, 0.0f });

        std::vector<char> delivered;
        float capturedImpulse = 0.0f;
        listener->SetBeginContactCallback([&](auto&, auto&, const Systems::ContactEventInfo& info)
        {
            delivered.push_back('B');
            assert(info.hasManifold);
            capturedImpulse = std::max(capturedImpulse, info.normalImpulse);
        });
        listener->SetEndContactCallback([&](auto&, auto&, const auto&)
        {
            delivered.push_back('E');
        });

        physics.GetBox2DSystem()->Apply(2.0f * kStep);
        assert(delivered.empty());
        listener->ProcessContacts();
        assert((delivered == std::vector<char>{ 'B' }));
        assert(capturedImpulse > 0.0f);
        physics.GetBox2DSystem()->Apply(0.0f);
        listener->ProcessContacts();
        assert(delivered.size() == 1);

        fruit->SetPositionViaPhysics({ 0.0f, 4.0f, 0.0f });
        physics.GetBox2DSystem()->Apply(kStep);
        fruit->SetPositionViaPhysics({ 0.0f, 0.9f, 0.0f });
        physics.GetBox2DSystem()->Apply(kStep);
        fruit->SetPositionViaPhysics({ 0.0f, 4.0f, 0.0f });
        physics.GetBox2DSystem()->Apply(kStep);
        listener->ProcessContacts();
        assert((delivered == std::vector<char>{ 'B', 'E', 'B', 'E' }));

        fruit->SetPositionViaPhysics({ 0.0f, 0.9f, 0.0f });
        physics.GetBox2DSystem()->Apply(kStep);
        listener->ClearDestroyedCaches();
        listener->ProcessContacts();
        assert(delivered.size() == 4);
    }

    void CheckDestructionDuringDispatch()
    {
        Systems::PhysicsTuningParameters tuning;
        tuning.gravity = 0.0f;
        Systems::PhysicsMachine physics(tuning);
        auto listener = std::make_shared<Systems::Box2DContactListener>(physics);
        physics.SetContactListener(listener);
        auto floorA = MakeBody(physics, -2.0f, 0.0f, b2_staticBody);
        auto floorB = MakeBody(physics, 2.0f, 0.0f, b2_staticBody);
        auto fruitA = MakeBody(physics, -2.0f, 0.9f, b2_dynamicBody);
        auto fruitB = MakeBody(physics, 2.0f, 0.9f, b2_dynamicBody);
        int callbacks = 0;
        listener->SetBeginContactCallback([&](auto&, auto&, const auto&)
        {
            ++callbacks;
            fruitA.reset();
            fruitB.reset();
        });
        physics.GetBox2DSystem()->Apply(kStep);
        listener->ProcessContacts();
        assert(callbacks == 1);
    }

    void CheckWeldRotation()
    {
        Systems::PhysicsTuningParameters tuning;
        tuning.gravity = 0.0f;
        Systems::PhysicsMachine physics(tuning);
        auto cake = MakeBody(physics, 0.0f, 0.0f, b2_staticBody, 0.4f);
        auto fruit = MakeBody(physics, 2.0f, 0.0f, b2_dynamicBody, -0.6f);
        const b2JointId joint = physics.CreateWeldJoint(
            *cake, *fruit, { 1.0f, 0.0f }, 0.0f, 0.7f, 0.0f, 0.7f);
        assert(b2Joint_IsValid(joint));
        for (int step = 0; step < 60; ++step)
        {
            physics.GetBox2DSystem()->Apply(kStep);
        }
        const float angle = b2Rot_GetAngle(b2Body_GetRotation(fruit->GetBox2DBodyId()));
        assert(std::fabs(angle + 0.6f) < 0.001f);
    }
}

int main()
{
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _CALL_REPORTFAULT);
    CheckContactCapture();
    CheckDestructionDuringDispatch();
    CheckWeldRotation();
    std::cout << "physics_regression: PASS\n";
}
