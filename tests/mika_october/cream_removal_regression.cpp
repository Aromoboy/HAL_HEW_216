// ミカ：既存の実物理テストを共用し、接着削除の前後で同じ条件を検証する
#define main RunCreamPhysicsRegression
#include "cream_physics_regression.cpp"
#undef main

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <string_view>

namespace
{
    void ReplaceBox(World& world, Entities::PhysicalEntity& entity, float halfWidth, int copies = 1)
    {
        world.physics.UnregisterBody(entity, true);
        Physics::BoxColliderComponent collider;
        for (int index = 0; index < copies; ++index)
        {
            collider.AddBox(halfWidth, 0.5f);
        }
        collider.shapeDef.enableContactEvents = true;
        entity.SetBoxColliderComponent(std::move(collider));
        world.physics.RegisterBody(entity);
    }

    std::vector<b2JointId> FruitWelds(const Entities::PhysicalEntity& fruit)
    {
        const b2BodyId body = fruit.GetBox2DBodyId();
        std::vector<b2JointId> result(static_cast<std::size_t>(b2Body_GetJointCount(body)));
        const int count = b2Body_GetJoints(body, result.data(), static_cast<int>(result.size()));
        result.resize(static_cast<std::size_t>(count));
        return result;
    }

    void DeliverImpact(World& world, SweetsSpace::ActiveSweets& glued, SweetsSpace::ActiveSweets& free)
    {
        // ミカ：実体への通知で関連の有無を確認し、実衝突は共通テストで検証する
        Systems::ContactEventInfo info{};
        info.hasManifold = true;
        info.manifold.normal = { 0.0f, 1.0f };
        info.normalImpulse = 1.0f;
        world.contacts.OnContactBegin(&glued, &free, info);
        assert(FruitWelds(glued).empty());
    }

    void Place(Entities::PhysicalEntity& entity, float x, float y)
    {
        entity.SetTransformViaPhysics({ x, y, 0.0f }, { 0.0f, 0.0f, 0.0f });
        entity.SetLinearVelocity({ 0.0f, 0.0f, 0.0f });
        entity.SetAngularVelocity(0.0f);
    }

    void CheckRemovalPaths()
    {
        for (int path = 0; path < 5; ++path)
        {
            // ミカ：接着後も接触を残す設定で同じ組に複数のジョイントを作る
            World world(0.01f, 0.0f, true);
            auto cake = MakeCake(world, 0.0f);
            ReplaceBox(world, *cake, 8.0f);
            auto cream = MakeSweets(world, "Cream", 0.95f);
            ReplaceBox(world, *cream, 0.5f, 2);
            auto fruit = MakeSweets(world, "Strawberry", 1.90f);
            auto otherCream = MakeSweets(world, "Cream", 0.95f);
            auto otherFruit = MakeSweets(world, "Blueberry", 1.90f);
            Place(*otherCream, 4.0f, 0.95f);
            Place(*otherFruit, 4.0f, 1.90f);
            world.Step(3);
            auto welds = FruitWelds(*fruit);
            assert(welds.size() == 2 && FruitWelds(*otherFruit).size() == 1);

            if (path == 0)
            {
                world.physics.DestroyJoint(welds.front());
                world.contacts.TickDebugStats(0.0f);
                assert(FruitWelds(*fruit).size() == 1);
                auto striker = MakeSweets(world, "Orange", 20.0f);
                DeliverImpact(world, *fruit, *striker);
            }
            else if (path == 1)
            {
                world.contacts.RemoveCreamWeldJointsForSweets(cream.get(), "removeEntry");
                assert(FruitWelds(*fruit).empty());
            }
            else if (path == 2)
            {
                world.contacts.RemoveCreamWeldJointsForSweets(fruit.get(), "TryMarkAsCarried");
                world.contacts.RemoveCreamWeldJointsForSweets(fruit.get(), "ReleaseCarriedSweets");
                assert(FruitWelds(*fruit).empty());
            }
            else if (path == 3)
            {
                world.physics.UnregisterBody(*fruit, true);
                world.contacts.TickDebugStats(0.0f);
                assert(world.contacts.BuildDebugSnapshot().activeFruitGlue == 1);
            }
            else
            {
                for (const auto joint : welds)
                {
                    world.physics.DestroyJoint(joint);
                }
                world.contacts.TickDebugStats(0.0f);
                assert(FruitWelds(*fruit).empty());
            }

            // ミカ：最後の接着を失ったフルーツを未接着として判定できることを確認する
            DeliverImpact(world, *otherFruit, *fruit);
            assert(world.contacts.BuildDebugSnapshot().activeFruitGlue == 0);
            world.contacts.ClearCreamWeldJoints("ClearAllSweets");
        }
        std::cout << "PASS: duplicate pair keeps remaining weld; impact, cream removal, carry and invalid body/joint remove final link\n";
    }

    void CheckClearAndRetry()
    {
        World world(0.01f, 0.0f, true);
        auto cake = MakeCake(world, 0.0f);
        auto cream = MakeSweets(world, "Cream", 0.95f);
        ReplaceBox(world, *cream, 0.5f, 2);
        auto fruit = MakeSweets(world, "Strawberry", 1.90f);
        auto striker = MakeSweets(world, "Blueberry", 20.0f);
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            Place(*cream, 0.0f, 0.95f);
            Place(*fruit, 0.0f, 1.90f);
            world.Step(3);
            assert(FruitWelds(*fruit).size() == 2);
            if (cycle == 0)
            {
                world.contacts.SetCreamEnabled(false);
                world.contacts.SetCreamEnabled(true);
            }
            else if (cycle == 1)
            {
                world.contacts.Reset();
                world.contacts.ClearCreamWeldJoints("ClearAllSweets");
            }
            else
            {
                DeliverImpact(world, *fruit, *striker);
                world.contacts.ClearCreamWeldJoints("ClearAllSweets");
            }
            assert(world.contacts.BuildDebugSnapshot().activeFruitGlue == 0);
            assert(world.contacts.BuildDebugSnapshot().activeCreamAnchors == 0);
            Place(*cream, 10.0f, 0.95f);
            Place(*fruit, 20.0f, 1.90f);
            world.Step(2);
        }
        std::cout << "PASS: disable, reset/clear and repeated creation reuse the same fruit/cream objects\n";
    }

    void CheckForceBreak()
    {
        World world(0.01f, 0.01f, true);
        auto cake = MakeCake(world, 0.0f);
        auto cream = MakeSweets(world, "Cream", 0.95f);
        ReplaceBox(world, *cream, 0.5f, 2);
        auto fruit = MakeSweets(world, "Strawberry", 1.90f);
        world.Step(1);
        assert(FruitWelds(*fruit).size() == 2);
        fruit->SetLinearVelocity({ 20.0f, 0.0f, 0.0f });
        world.Step(1);
        assert(FruitWelds(*fruit).empty());
        assert(world.contacts.BuildDebugSnapshot().activeFruitGlue == 0);
        world.contacts.ClearCreamWeldJoints("ClearAllSweets");
        std::cout << "PASS: force break removes all multiple-collider welds\n";
    }

    void BenchmarkMassRemoval(int count)
    {
        std::vector<double> samples;
        for (int sample = 0; sample < 11; ++sample)
        {
            World world;
            auto cake = MakeCake(world, 0.0f);
            ReplaceBox(world, *cake, static_cast<float>(count));
            auto cream = MakeSweets(world, "Cream", 0.95f);
            ReplaceBox(world, *cream, static_cast<float>(count));
            std::vector<std::unique_ptr<SweetsSpace::ActiveSweets>> fruits;
            for (int index = 0; index < count; ++index)
            {
                auto fruit = MakeSweets(world, "Strawberry", 1.90f);
                Place(*fruit, (index - count * 0.5f) * 1.1f, 1.90f);
                fruits.push_back(std::move(fruit));
            }
            world.Step(1);
            assert(world.contacts.BuildDebugSnapshot().activeFruitGlue == count);
            const auto start = std::chrono::steady_clock::now();
            world.contacts.RemoveCreamWeldJointsForSweets(cream.get(), "removeEntry");
            const auto end = std::chrono::steady_clock::now();
            assert(world.contacts.BuildDebugSnapshot().activeFruitGlue == 0);
            assert(world.contacts.BuildDebugSnapshot().activeCreamAnchors == 0);
            for (const auto& fruit : fruits)
            {
                assert(FruitWelds(*fruit).empty());
            }
            if (sample >= 2)
            {
                samples.push_back(std::chrono::duration<double, std::milli>(end - start).count());
            }
        }
        std::sort(samples.begin(), samples.end());
        std::cout << std::fixed << std::setprecision(4) << "BENCH mass_remove joints=" << count
            << " samples=" << samples.size() << " median_ms=" << samples[samples.size() / 2]
            << " max_ms=" << samples.back() << '\n';
    }
}

int main(int argc, char** argv)
{
    RunCreamPhysicsRegression();
    CheckRemovalPaths();
    CheckClearAndRetry();
    CheckForceBreak();
    if (argc > 1 && std::string_view(argv[1]) == "--benchmark")
    {
        for (int count : { 100, 500, 1000 })
        {
            BenchmarkMassRemoval(count);
        }
    }
    std::cout << "cream_removal_regression: PASS\n";
}
