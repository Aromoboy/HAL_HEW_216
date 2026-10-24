#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

#include "Sweets/BaseSweets.h"
#include "Sweets/SweetsContacts.h"
#include "Systems/Physics/Box2DContactListener.h"
#include "Systems/Physics/Box2DPhysicsSystem.h"
#include "Systems/Physics/PhysicsMachine.h"

// ミカ：描画だけを置き換え、誤って呼ばれた場合はテストを失敗させる
void ModelRender::Draw(Render::Model&, Transform, const CameraSpace::ICamera*, Animation*)
{
    std::abort();
}

namespace
{
    constexpr float kStep = 1.0f / 60.0f;

    Systems::PhysicsTuningParameters MakeTuning(float breakImpulse, float breakForce = 0.0f,
        bool collideConnected = false)
    {
        Systems::PhysicsTuningParameters tuning;
        tuning.gravity = 0.0f;
        tuning.creamGlueBreakImpulse = breakImpulse;
        tuning.creamGlueBreakForce = breakForce;
        tuning.creamGlueCollideConnected = collideConnected;
        return tuning;
    }

    struct World
    {
        Systems::PhysicsMachine physics;
        std::shared_ptr<Systems::Box2DContactListener> listener;
        bool warnedMissingPhysics = false;
        SweetsSpace::SweetsContacts contacts;

        explicit World(float breakImpulse = 0.0f, float breakForce = 0.0f, bool collideConnected = false)
            : physics(MakeTuning(breakImpulse, breakForce, collideConnected))
            , listener(std::make_shared<Systems::Box2DContactListener>(physics))
            , contacts(&physics, &warnedMissingPhysics)
        {
            physics.SetContactListener(listener);
            listener->SetBeginContactCallback([this](auto& a, auto& b, const auto& info)
            {
                contacts.OnContactBegin(&a, &b, info);
            });
            listener->SetEndContactCallback([this](auto& a, auto& b, const auto& info)
            {
                contacts.OnContactEnd(&a, &b, info);
            });
        }

        void Step(int count = 1)
        {
            for (int step = 0; step < count; ++step)
            {
                // ミカ：実ゲームと同じ物理、接触配信、遅延接着の順で進める
                physics.GetBox2DSystem()->Apply(kStep);
                listener->ProcessContacts();
                contacts.TickDebugStats(kStep);
                contacts.ProcessPendingGlue();
            }
        }
    };

    void AttachBox(Entities::PhysicalEntity& entity, World& world)
    {
        Physics::BoxColliderComponent collider;
        collider.AddBox(0.5f, 0.5f);
        collider.shapeDef.enableContactEvents = true;
        entity.SetBoxColliderComponent(std::move(collider));
        entity.AttachPhysicsMachine(&world.physics);
        assert(entity.HasBox2DBody());
    }

    std::unique_ptr<SweetsSpace::ActiveSweets> MakeSweets(World& world, const char* tag, float y)
    {
        Transform transform{};
        transform.position = { 0.0f, y, 0.0f };
        transform.scale = { 1.0f, 1.0f, 1.0f };
        Physics::Box2DShapeConfig shape;
        shape.type = Physics::Box2DShapeConfig::Type::Box;
        auto entity = std::make_unique<SweetsSpace::ActiveSweets>(
            tag, transform, nullptr, shape, Collision::CollisionLayer::Sweets);
        entity->SetWasEverCarried(true);
        AttachBox(*entity, world);
        return entity;
    }

    std::unique_ptr<Entities::PhysicalEntity> MakeCake(World& world, float y)
    {
        Transform transform{};
        transform.position = { 0.0f, y, 0.0f };
        transform.scale = { 1.0f, 1.0f, 1.0f };
        Physics::Box2DShapeConfig shape;
        shape.type = Physics::Box2DShapeConfig::Type::Box;
        shape.bodyType = b2_staticBody;
        auto entity = std::make_unique<Entities::PhysicalEntity>(transform, shape,
            Collision::CollisionLayer::Floor, nullptr, nullptr, Core::UpdatePhase::PrePhysics, 0, "cake");
        AttachBox(*entity, world);
        return entity;
    }

    b2JointId FindWeld(const Entities::PhysicalEntity& a, const Entities::PhysicalEntity& b)
    {
        const b2BodyId body = a.GetBox2DBodyId();
        const int capacity = b2Body_GetJointCount(body);
        std::vector<b2JointId> joints(static_cast<std::size_t>(capacity));
        const int count = b2Body_GetJoints(body, joints.data(), capacity);
        for (int index = 0; index < count; ++index)
        {
            const b2JointId joint = joints[index];
            if (b2Joint_GetType(joint) == b2_weldJoint
                && (B2_ID_EQUALS(b2Joint_GetBodyA(joint), b.GetBox2DBodyId())
                    || B2_ID_EQUALS(b2Joint_GetBodyB(joint), b.GetBox2DBodyId())))
            {
                return joint;
            }
        }
        return b2_nullJointId;
    }

    void CheckFallingAssembly(World& world, const std::vector<SweetsSpace::ActiveSweets*>& group,
        const std::vector<b2JointId>& welds)
    {
        const b2BodyId reference = group.front()->GetBox2DBodyId();
        std::vector<b2Vec2> positions;
        std::vector<b2Vec2> relativePositions;
        std::vector<float> relativeAngles;
        for (auto* sweets : group)
        {
            const b2BodyId body = sweets->GetBox2DBodyId();
            assert(b2Body_GetType(body) == b2_dynamicBody);
            assert(b2Body_GetGravityScale(body) > 0.0f);
            positions.push_back(b2Body_GetPosition(body));
            relativePositions.push_back(b2Body_GetLocalPoint(reference, positions.back()));
            relativeAngles.push_back(b2Rot_GetAngle(
                b2InvMulRot(b2Body_GetRotation(reference), b2Body_GetRotation(body))));
        }
        world.Step(60);
        for (std::size_t index = 0; index < group.size(); ++index)
        {
            const b2BodyId body = group[index]->GetBox2DBodyId();
            assert(b2Body_GetType(body) == b2_dynamicBody);
            const b2Vec2 position = b2Body_GetPosition(body);
            assert(position.y < positions[index].y - 1.0f);
            assert(b2Distance(b2Body_GetLocalPoint(reference, position), relativePositions[index]) < 0.03f);
            const float relativeAngle = b2Rot_GetAngle(
                b2InvMulRot(b2Body_GetRotation(reference), b2Body_GetRotation(body)));
            assert(std::fabs(relativeAngle - relativeAngles[index]) < 0.03f);
            assert(!world.contacts.IsAnchoredToCake(*group[index]));
        }
        for (b2JointId weld : welds)
        {
            assert(b2Joint_IsValid(weld));
        }
        const auto snapshot = world.contacts.BuildDebugSnapshot();
        assert(snapshot.activeFruitGlue == static_cast<int>(welds.size()));
        assert(snapshot.activeCreamAnchors == 0 && snapshot.frozenFruit == 0);
    }

    void CheckIndirectSupportAndBodyRemoval()
    {
        World world;
        auto supportFruit = MakeSweets(world, "Strawberry", 0.95f);
        auto cream = MakeSweets(world, "Cream", 1.90f);
        auto topFruit = MakeSweets(world, "Blueberry", 2.85f);
        world.Step(3);
        assert(world.contacts.BuildDebugSnapshot().activeFruitGlue == 0);
        assert(world.contacts.GetCakeAnchoredSweets().empty());
        assert(b2Body_GetJointCount(cream->GetBox2DBodyId()) == 0);

        auto cake = MakeCake(world, 0.0f);
        world.Step(3);
        const b2JointId lowerWeld = FindWeld(*cream, *supportFruit);
        const b2JointId upperWeld = FindWeld(*cream, *topFruit);
        assert(b2Joint_IsValid(lowerWeld) && b2Joint_IsValid(upperWeld));
        const auto anchored = world.contacts.GetCakeAnchoredSweets();
        assert(anchored.contains(cream.get()) && anchored.contains(topFruit.get()));
        const auto snapshot = world.contacts.BuildDebugSnapshot();
        assert(snapshot.activeCreamAnchors == 0 && snapshot.activeFruitGlue == 2);
        world.physics.SetGravity(9.8f);
        world.Step(120);
        cake.reset();
        CheckFallingAssembly(world, { cream.get(), supportFruit.get(), topFruit.get() }, { lowerWeld, upperWeld });
        std::cout << "PASS: isolated cream waits, indirect cake support glues, removed support body lets intact group fall\n";
    }

    void CheckAnchorJointRemoval()
    {
        World world;
        auto cake = MakeCake(world, 0.0f);
        auto cream = MakeSweets(world, "Cream", 0.95f);
        auto fruit = MakeSweets(world, "Strawberry", 1.90f);
        world.Step(3);
        const b2JointId anchor = FindWeld(*cream, *cake);
        const b2JointId fruitWeld = FindWeld(*cream, *fruit);
        assert(b2Joint_IsValid(anchor) && b2Joint_IsValid(fruitWeld));
        world.physics.SetGravity(9.8f);
        world.Step(120);
        world.physics.DestroyJoint(anchor);
        cake->SetPositionViaPhysics({ -10.0f, 0.0f, 0.0f });
        CheckFallingAssembly(world, { cream.get(), fruit.get() }, { fruitWeld });
        std::cout << "PASS: removed cake anchor lets dynamic group fall while fruit weld survives\n";
    }

    void CheckImpactDoesNotAddVelocity()
    {
        World world(0.01f);
        auto cake = MakeCake(world, 0.0f);
        auto cream = MakeSweets(world, "Cream", 0.95f);
        auto fruit = MakeSweets(world, "Strawberry", 1.90f);
        world.Step(3);
        const b2JointId fruitWeld = FindWeld(*cream, *fruit);
        assert(b2Joint_IsValid(fruitWeld));
        assert(b2Body_GetType(fruit->GetBox2DBodyId()) == b2_dynamicBody);
        auto striker = MakeSweets(world, "Blueberry", 4.0f);
        striker->SetLinearVelocity({ 0.0f, -12.0f, 0.0f });
        bool impactChecked = false;
        world.listener->SetBeginContactCallback([&](auto& a, auto& b, const auto& info)
        {
            const bool impact = (&a == fruit.get() && &b == striker.get())
                || (&b == fruit.get() && &a == striker.get());
            const b2Vec2 before = b2Body_GetLinearVelocity(fruit->GetBox2DBodyId());
            world.contacts.OnContactBegin(&a, &b, info);
            if (impact)
            {
                assert(info.hasManifold && std::fabs(info.manifold.normal.y) * info.normalImpulse > 0.01f);
                assert(!b2Joint_IsValid(fruitWeld));
                const b2Vec2 after = b2Body_GetLinearVelocity(fruit->GetBox2DBodyId());
                assert(b2Distance(before, after) < 0.00001f);
                impactChecked = true;
            }
        });
        for (int step = 0; step < 30 && !impactChecked; ++step)
        {
            world.Step();
        }
        assert(impactChecked);
        std::cout << "PASS: real impact breaks fruit weld without adding a second impulse\n";
    }
}

int main()
{
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _CALL_REPORTFAULT);
    std::cout << std::unitbuf;
    CheckIndirectSupportAndBodyRemoval();
    CheckAnchorJointRemoval();
    CheckImpactDoesNotAddVelocity();
    std::cout << "cream_physics_regression: PASS\n";
    return 0;
}
