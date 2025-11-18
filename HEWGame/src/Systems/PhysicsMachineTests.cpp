// ============================================================================
// 概要: PhysicsMachineTests の挙動や関連ロジックを実装し、src／Systems に属する機能を具体化している。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: src／Systems 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#include "Systems/PhysicsMachine.h"
#include "Systems/IntegrateSystem.h"
#include "Systems/ContactLiteSystem.h"
#include "GravityDampingSystem.h"
#include "AABBCollider.h"
#include "Core/StepTimer.h"
#include "Core/Input.h"

#include <cassert>
#include <memory>
#include <string>

namespace
{
    SceneSpace::PhysicalEntity CreateDynamicBody(const std::string& tag, const Transform& transform, float weight, float damping)
    {
        auto collider = std::make_shared<Collision::AABBCollider>(transform, false, 1.0f, float3{ 0.0f, 0.0f, 0.0f });
        return SceneSpace::PhysicalEntity(tag,
            transform.position,
            transform.rotation,
            transform.scale,
            collider,
            float3{ 0.0f, 0.0f, 0.0f },
            float3{ 0.0f, 0.0f, 0.0f },
            weight,
            damping,
            Core::UpdatePhase::Physics,
            0);
    }

    void RunIntegrationStepMovesBodiesTest()
    {
        Systems::PhysicsMachine machine;
        auto integrator = std::make_shared<Systems::IntegrateSystem>();
        auto gravity = std::make_shared<Systems::GravityDampingSystem>(9.8f, 0.98f);
        machine.SetSystem(gravity);
        machine.SetIntegrator(integrator);

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
        assert(updatedY < initialY);
#endif

        machine.UnregisterBody(falling);
    }

    void RunContactStopsVerticalVelocityTest()
    {
        Systems::PhysicsMachine machine;
        auto integrator = std::make_shared<Systems::IntegrateSystem>();
        auto gravity = std::make_shared<Systems::GravityDampingSystem>(9.8f, 0.98f);
        machine.SetSystem(gravity);
        machine.SetIntegrator(integrator);

        Systems::ContactLiteSystem contactSystem;
        contactSystem.SetBodySource(&machine.GetBodies());

        Transform platformTransform{};
        platformTransform.position = float3{ 0.0f, -1.0f, 0.0f };
        platformTransform.rotation = float3{ 0.0f, 0.0f, 0.0f };
        platformTransform.scale = float3{ 4.0f, 1.0f, 4.0f };
        auto platform = CreateDynamicBody("TestPlatform", platformTransform, 0.0f, 1.0f);

        Transform fallingTransform{};
        fallingTransform.position = float3{ 0.0f, -0.2f, 0.0f };
        fallingTransform.rotation = float3{ 0.0f, 0.0f, 0.0f };
        fallingTransform.scale = float3{ 1.0f, 1.0f, 1.0f };
        auto falling = CreateDynamicBody("TestFallingSweet", fallingTransform, 1.0f, 0.95f);

        machine.RegisterBody(platform);
        machine.RegisterBody(falling);

        Core::StepTimer timer;
        Core::Input dummyInput;
        Core::UpdateContext ctx(0.016f, timer, dummyInput, nullptr);

        auto& phys = falling.GetPhysical();
        phys.velocity = float3{ 0.0f, -10.0f, 0.0f };

        machine.OnUpdate(ctx);
        contactSystem.OnUpdate(ctx);

#if defined(_DEBUG)
        assert(phys.velocity.y >= -1e-4f);
#endif

        machine.UnregisterBody(platform);
        machine.UnregisterBody(falling);
    }

    struct PhysicsMachineTestRunner
    {
        PhysicsMachineTestRunner()
        {
            RunIntegrationStepMovesBodiesTest();
            RunContactStopsVerticalVelocityTest();
        }
    };

    static PhysicsMachineTestRunner g_physicsMachineTestRunner;
}
