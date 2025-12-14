#if defined(_DEBUG) && defined(PHYSICS_SELF_TEST)
#include "Systems/PhysicsMachine.h"
#include "Entities/PhysicalEntity.h"
#include "BoxColliderComponent.h"
#include "Core/StepTimer.h"
#include "Core/Input.h"

#include <cassert>
#include <memory>
#include <optional>
#include <string>

namespace
{

    // Box2Dで動作する動的ボディを生成する簡易ユーティリティ
    // 旧物理API（ConvexHullCollider2D、weight、damping等）は完全に削除されたため使用不可
    Entities::PhysicalEntity CreateDynamicBody(const std::string& tag, const Transform& transform)
    {

        // PhysicalEntityの新しいコンストラクタはBox2Dの形状設定を直接受け取る
        // Transform + ColliderLayer + UpdatePhaseで十分
        Entities::PhysicalEntity entity(
            tag,
            transform.position,
            transform.rotation,
            transform.scale,
            std::nullopt,
            Collision::CollisionLayer::Default,
            Core::UpdatePhase::Physics,
            0
        );


        // BoxColliderComponentで簡単な1×1の四角形コライダーを作成
        Physics::BoxColliderComponent collider;
        collider.shapeDef.material.friction = 0.6f;
        collider.shapeDef.material.restitution = 0.05f;
        collider.AddBox(
            0.5f,    // half-width
            0.5f,    // half-height
            b2Vec2{0.0f, 0.0f} // offset
        );

        entity.SetBoxColliderComponent(std::move(collider));

        return entity;
    }


    // Box2Dによるステップが正しくY座標を変化させるかテストする
    void RunBox2DStepMovesBodiesTest()
    {

        // 重力を設定したPhysicsMachineを作成
        Systems::PhysicsTuningParameters tuning{};
        tuning.gravity = 9.8f;
        Systems::PhysicsMachine machine(tuning);

        // 落下開始位置
        Transform fallingTransform{};
        fallingTransform.position = float3{ 0.0f, 5.0f, 0.0f };
        fallingTransform.rotation = float3{ 0.0f, 0.0f, 0.0f };
        fallingTransform.scale    = float3{ 1.0f, 1.0f, 1.0f };

        // テスト用動的ボディ作成
        auto falling = CreateDynamicBody("PhysicsTestSweet", fallingTransform);


        // ボディを登録（Box2Dボディが内部で生成される）
        machine.RegisterBody(falling);


        // Box2DボディID取得（デバッグのみ使用）
#if defined(_DEBUG)
        const b2BodyId bodyId = machine.FindBody(falling);
        assert(b2Body_IsValid(bodyId));
#endif


        // 初期位置を保存
        const float initialY = falling.GetTransform().position.y;


        // タイマーと入力（ダミー）
        Core::StepTimer timer;
        Core::Input dummyInput;
        Core::UpdateContext ctx(0.016f, timer, dummyInput, nullptr);

        // 速度を設定する: Box2DのAPIを使う必要がある
#if defined(_DEBUG)
        b2Body_SetLinearVelocity(bodyId, b2Vec2{ 0.0f, -4.0f });
#endif

        // 1ステップ更新
        machine.OnUpdate(ctx);

        // 更新後のYを取得し、落下しているか確認
        const float updatedY = falling.GetTransform().position.y;

#if defined(_DEBUG)
        assert(updatedY < initialY); // Y位置が低くなる＝落下成功
#endif

        // ボディ解除
        machine.UnregisterBody(falling);

#if defined(_DEBUG)
        assert(!b2Body_IsValid(machine.FindBody(falling)));
#endif
    }

    // テスト自動実行用ランナー
    struct PhysicsMachineTestRunner
    {
        PhysicsMachineTestRunner()
        {
            RunBox2DStepMovesBodiesTest();
        }
    };

    static PhysicsMachineTestRunner g_physicsMachineTestRunner;
}
#endif