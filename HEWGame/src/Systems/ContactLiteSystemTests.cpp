#if defined(ENABLE_CONTACT_LITE_TESTS)

#include "Systems/ContactLiteSystem.h"

#include "Collision2D.h"
#include "Core/Input.h"
#include "Core/UpdateContext.h"
#include "Core/StepTimer.h"
#include "Physics.h"

#include <cassert>
#include <memory>
#include <vector>

namespace
{
        using Systems::ContactLiteSystem;

        Entities::PhysicalEntity BuildPhysical(
                const Transform& transform,
                std::shared_ptr<Collision::ICollider> collider,
                Collision::CollisionLayer layer,
                Physics::Physical physical,
                ContactLiteSystem& contactSystem,
                const char* tag)
        {
                return Entities::PhysicalEntity(
                        transform,
                        std::move(collider),
                        layer,
                        physical,
                        nullptr,
                        nullptr,
                        &contactSystem,
                        Core::UpdatePhase::PrePhysics,
                        0,
                        tag);
        }

        void RunSweetsFoundationRestingTest()
        {
                ContactLiteSystem contactSystem;

                Transform foundationTransform{ float3{ 0.0f, 0.0f, 0.0f }, float3{ 0.0f, 0.0f, 0.0f }, float3{ 4.0f, 1.0f, 1.0f } };
                std::vector<float2> foundationPoints
                {
                        float2{ -2.0f, -0.5f },
                        float2{ 2.0f, -0.5f },
                        float2{ 2.0f, 0.5f },
                        float2{ -2.0f, 0.5f },
                };
                auto foundationCollider = std::make_shared<Collision::ConvexHullCollider2D>(
                        foundationPoints,
                        float2{ 0.0f, 0.0f },
                        foundationTransform.scale.z,
                        Collision::CollisionLayer::Floor);
                Physics::Physical foundationPhysical{};
                auto foundation = BuildPhysical(
                        foundationTransform,
                        foundationCollider,
                        Collision::CollisionLayer::Floor,
                        foundationPhysical,
                        contactSystem,
                        "StageFoundation");
                foundationCollider->UpdateBounds(foundationTransform);

                Transform sweetsTransform{ float3{ 0.0f, 0.75f, 0.0f }, float3{ 0.0f, 0.0f, 0.0f }, float3{ 1.0f, 1.0f, 1.0f } };
                std::vector<float2> sweetsPoints
                {
                        float2{ -0.5f, -0.5f },
                        float2{ 0.5f, -0.5f },
                        float2{ 0.5f, 0.5f },
                        float2{ -0.5f, 0.5f },
                };
                auto sweetsCollider = std::make_shared<Collision::ConvexHullCollider2D>(
                        sweetsPoints,
                        float2{ 0.0f, 0.0f },
                        sweetsTransform.scale.z,
                        Collision::CollisionLayer::Sweets);
                Physics::Physical sweetsPhysical{};
                auto sweets = BuildPhysical(
                        sweetsTransform,
                        sweetsCollider,
                        Collision::CollisionLayer::Sweets,
                        sweetsPhysical,
                        contactSystem,
                        "Cake");
                sweetsCollider->UpdateBounds(sweetsTransform);

                std::vector<Entities::PhysicalEntity*> bodies{ &foundation, &sweets };
                contactSystem.SetBodySource(&bodies);

                Core::StepTimer timer;
                Core::Input input;
                Core::UpdateContext context(0.0f, timer, input, nullptr);

                contactSystem.OnUpdate(context);

                const AABB foundationBounds = foundation.GetCollider()->GetAABB();
                const AABB sweetsBounds = sweets.GetCollider()->GetAABB();

                const bool sweetsAboveFoundation = sweetsBounds.min.y >= foundationBounds.max.y - 1e-4f;
                const bool contactRegistered = contactSystem.HadAnyContactThisFrame(&sweets);

#if defined(_DEBUG)
                assert(sweetsAboveFoundation);
                assert(contactRegistered);
#endif

                (void)sweetsAboveFoundation;
                (void)contactRegistered;
        }

        void RunAllContactLiteSystemTests()
        {
                RunSweetsFoundationRestingTest();
        }
}

namespace Systems::Tests
{
        void RunContactLiteSystemTests()
        {
                RunAllContactLiteSystemTests();
        }
}

#endif // defined(ENABLE_CONTACT_LITE_TESTS)
