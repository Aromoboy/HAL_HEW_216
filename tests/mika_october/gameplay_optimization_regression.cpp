// ミカ：既存の実物理テスト環境を共有し、同じ入力で最適化前後を測る
#define main RunCreamPhysicsRegression
#include "cream_physics_regression.cpp"
#undef main

#include <algorithm>
#include <chrono>
#include <iomanip>

namespace
{
#ifdef MIKA_OPTIMIZED
    void CheckDirectDefinitionReplacement()
    {
        World world;
        auto cake = MakeCake(world, 0.0f);
        auto fruit = MakeSweets(world, "Strawberry", 0.95f);
        world.Step(3);
        world.physics.SyncBodies(kStep);
        assert(world.contacts.GetCakeAnchoredSweets().contains(fruit.get()));
        fruit->SyncColliderBounds();
        const float oldWidth = fruit->GetAABB().max.x - fruit->GetAABB().min.x;
        const b2BodyId oldBody = fruit->GetBox2DBodyId();
        const float oldMass = b2Body_GetMass(oldBody);
        auto definition = fruit->GetBox2DShapeConfig();
        definition->density *= 2.0f;
        fruit->SetBox2DShapeConfig(definition);
        const b2BodyId newBody = fruit->GetBox2DBodyId();
        assert(b2Body_IsValid(newBody) && !B2_ID_EQUALS(oldBody, newBody));
        assert(b2Body_GetShapeCount(newBody) == 1);
        assert(std::fabs(b2Body_GetMass(newBody) - oldMass * 2.0f) < 0.001f);
        fruit->SyncColliderBounds();
        assert(std::fabs(fruit->GetAABB().max.x - fruit->GetAABB().min.x - oldWidth) < 0.001f);
        world.Step(3);
        assert(world.contacts.GetCakeAnchoredSweets().contains(fruit.get()));
        std::cout << "PASS: direct definition replacement preserves fixtures, density, bounds and contacts\n";
    }
#endif

    void CheckCachedQueriesFollowMutations()
    {
        World world;
        world.physics.SetGravity(9.8f);
        auto cake = MakeCake(world, 0.0f);
        auto fruit = MakeSweets(world, "Strawberry", 0.95f);
        world.Step(3);
        assert(world.contacts.GetCakeAnchoredSweets().contains(fruit.get()));
        assert(world.contacts.GetCakeAnchoredSweets().contains(fruit.get()));
        fruit->SyncColliderBounds();
        const AABB first = fruit->GetAABB();
        for (int repeat = 0; repeat < 10; ++repeat)
        {
            fruit->SyncColliderBounds();
            assert(fruit->GetAABB().min.x == first.min.x);
        }

        // ミカ：直接の物理移動、回転、定義差し替えも境界に反映されることを確認する
        const b2BodyId originalBody = fruit->GetBox2DBodyId();
        b2Body_SetTransform(originalBody, { 10.0f, 8.0f }, b2MakeRot(0.3f));
        fruit->SyncColliderBounds();
        assert(fruit->GetAABB().min.x > 9.0f && fruit->GetAABB().min.y > 7.0f);
        world.Step(2);
        assert(!world.contacts.GetCakeAnchoredSweets().contains(fruit.get()));
        fruit->SetTransformViaPhysics({ 0.0f, 0.95f, 0.0f }, { 0.0f, 0.0f, 0.0f });
        fruit->SetLinearVelocity({ 0.0f, 0.0f, 0.0f });
        fruit->SetAngularVelocity(0.0f);
        world.Step(3);
        assert(world.contacts.GetCakeAnchoredSweets().contains(fruit.get()));
        const auto definition = fruit->GetBox2DShapeConfig();
        world.physics.UnregisterBody(*fruit, true);
        fruit->SetBox2DShapeConfig(definition);
        fruit->SetTransformViaPhysics({ 0.0f, 0.95f, 0.0f }, { 0.0f, 0.0f, 0.0f });
        fruit->SyncColliderBounds();
        assert(!B2_ID_EQUALS(originalBody, fruit->GetBox2DBodyId()));
        world.Step(3);
        assert(world.contacts.GetCakeAnchoredSweets().contains(fruit.get()));

        // ミカ：保持によるボディ解除と再登録、削除待ち、リセット後に古い到達結果を残さない
        world.physics.UnregisterBody(*fruit, true);
        assert(!world.contacts.GetCakeAnchoredSweets().contains(fruit.get()));
        world.physics.RegisterBody(*fruit);
        fruit->SetTransformViaPhysics({ 0.0f, 0.95f, 0.0f }, { 0.0f, 0.0f, 0.0f });
        world.Step(3);
        assert(world.contacts.GetCakeAnchoredSweets().contains(fruit.get()));
        world.contacts.ClearSweetsContact(fruit.get());
        fruit->MarkPendingDestroy();
        assert(!world.contacts.GetCakeAnchoredSweets().contains(fruit.get()));
        world.contacts.Reset();
        assert(!world.contacts.GetCakeAnchoredSweets().contains(fruit.get()));
        auto replacement = MakeSweets(world, "Blueberry", 0.95f);
        world.physics.UnregisterBody(*fruit);
        world.Step(3);
        assert(world.contacts.GetCakeAnchoredSweets().contains(replacement.get()));
        std::cout << "PASS: repeated graph/bounds queries follow move, definition, carry, delete and reset\n";
    }

    void BenchmarkSettledFruit(int count)
    {
        World world;
        auto cake = MakeCake(world, 0.0f);
        world.physics.UnregisterBody(*cake);
        Physics::BoxColliderComponent collider;
        collider.AddBox(static_cast<float>(count), 0.5f);
        collider.shapeDef.enableContactEvents = true;
        cake->SetBoxColliderComponent(std::move(collider));
        world.physics.RegisterBody(*cake);
        std::vector<std::unique_ptr<SweetsSpace::ActiveSweets>> fruits;
        fruits.reserve(static_cast<std::size_t>(count));
        for (int index = 0; index < count; ++index)
        {
            auto fruit = MakeSweets(world, "Strawberry", 0.95f);
            fruit->SetPositionViaPhysics({ (index - count * 0.5f) * 1.05f, 0.95f, 0.0f });
            fruits.push_back(std::move(fruit));
        }
        world.Step(4);
        for (const auto& fruit : fruits)
        {
            b2Body_SetAwake(fruit->GetBox2DBodyId(), false);
        }
        std::vector<double> graphMilliseconds;
        std::vector<double> boundsMilliseconds;
        std::size_t checksum = 0;
        for (int frame = 0; frame < 140; ++frame)
        {
            world.physics.GetBox2DSystem()->Apply(kStep);
            world.listener->ProcessContacts();
            const auto graphStart = std::chrono::steady_clock::now();
            for (int query = 0; query < 3; ++query)
            {
                const auto& anchored = world.contacts.GetCakeAnchoredSweets();
                assert(anchored.size() == static_cast<std::size_t>(count));
                checksum += anchored.size();
            }
            const auto graphEnd = std::chrono::steady_clock::now();
            for (int pass = 0; pass < 4; ++pass)
            {
                for (const auto& fruit : fruits)
                {
                    fruit->SyncColliderBounds();
                    assert(fruit->GetAABB().max.y > 0.5f);
                    if (pass == 0)
                    {
                        checksum += world.contacts.HasSupportingContact(*fruit);
                    }
                }
            }
            const auto boundsEnd = std::chrono::steady_clock::now();
            if (frame >= 20)
            {
                graphMilliseconds.push_back(std::chrono::duration<double, std::milli>(graphEnd - graphStart).count());
                boundsMilliseconds.push_back(std::chrono::duration<double, std::milli>(boundsEnd - graphEnd).count());
            }
        }
        const auto percentile = [](std::vector<double> samples, double fraction)
        {
            std::sort(samples.begin(), samples.end());
            return samples[static_cast<std::size_t>((samples.size() - 1) * fraction)];
        };
        std::cout << std::fixed << std::setprecision(4)
            << "BENCH fruits=" << count << " samples=120 graph_queries=3 bounds_passes=4"
            << " graph_p50_ms=" << percentile(graphMilliseconds, 0.50)
            << " graph_p95_ms=" << percentile(graphMilliseconds, 0.95)
            << " bounds_contact_p50_ms=" << percentile(boundsMilliseconds, 0.50)
            << " bounds_contact_p95_ms=" << percentile(boundsMilliseconds, 0.95)
            << " checksum=" << checksum << '\n';
    }
}

int main()
{
    RunCreamPhysicsRegression();
#ifdef MIKA_OPTIMIZED
    CheckDirectDefinitionReplacement();
#endif
    CheckCachedQueriesFollowMutations();
    for (int count : { 100, 500, 1000 })
    {
        BenchmarkSettledFruit(count);
    }
    std::cout << "gameplay_optimization_regression: PASS\n";
    return 0;
}
