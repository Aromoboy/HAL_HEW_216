#include "Systems/PhysicsMachine.h"
#include "Collision2D.h"
#include "Core/StepTimer.h"
#include "Core/Input.h"

#include <cassert>
#include <memory>
#include <string>

namespace
{
    Entities::PhysicalEntity CreateDynamicBody(const std::string& tag, const Transform& transform, float weight, float damping)
    {
        std::vector<float2> points
        {
            float2{ -0.5f, -0.5f },
            float2{ 0.5f, -0.5f },
            float2{ 0.5f, 0.5f },
            float2{ -0.5f, 0.5f },
        };
        auto collider = std::make_shared<Collision::ConvexHullCollider2D>(points, float2{ 0.0f, 0.0f }, transform.scale.z, Collision::CollisionLayer::Default);
        collider->UpdateBounds(transform);
        Entities::PhysicalEntity entity(tag,
            transform.position,
            transform.rotation,
            transform.scale,
            collider,
            Collision::CollisionLayer::Default,
            Core::UpdatePhase::Physics,
            0);

        auto& physical = entity.GetPhysical();
        physical.weight = weight;
        physical.damping = damping;

        return entity;
    }

    void RunBox2DStepMovesBodiesTest()
    {
        Systems::PhysicsMachine machine(9.8f);

        Transform fallingTransform{};
        fallingTransform.position = float3{ 0.0f, 5.0f, 0.0f };
        fallingTransform.rotation = float3{ 0.0f, 0.0f, 0.0f };
        fallingTransform.scale = float3{ 1.0f, 1.0f, 1.0f };
        auto falling = CreateDynamicBody("PhysicsTestSweet", fallingTransform, 1.0f, 0.95f);

        machine.RegisterBody(falling);

        Core::StepTimer timer;
        Core::Input dummyInput;
        Core::UpdateContext ctx(0.016f, timer, dummyInput, nullptr);

        auto& phys = falling.GetPhysical();
        phys.velocity = float3{ 0.0f, -4.0f, 0.0f };

        const float initialY = falling.GetTransform().position.y;
        machine.OnUpdate(ctx);
        const float updatedY = falling.GetTransform().position.y;

#if defined(_DEBUG)
        const b2BodyId bodyId = machine.FindBody(falling);
        assert(b2Body_IsValid(bodyId));
        assert(updatedY < initialY);
#endif

        machine.UnregisterBody(falling);

#if defined(_DEBUG)
        assert(machine.FindBody(falling) == b2_nullBodyId);
#endif
    }

    struct PhysicsMachineTestRunner
    {
        PhysicsMachineTestRunner()
        {
            RunBox2DStepMovesBodiesTest();
        }
    };

    static PhysicsMachineTestRunner g_physicsMachineTestRunner;
}
