// プレイヤー本体の動きと持ち上げをまとめたクラスだ
// 入力に合わせて移動や向きが変わり拾ったスイーツの状態も変える
// 取得した判定結果はスイーツ側やゲーム進行の判断に使われる
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Entities/PhysicalEntity.h"
#include "Entities/PlayerModel.h"
#include "Entities/PlayerMovement.h"
#include "Entities/PlayerPhysicsBinder.h"
#include "Entities/PlayerPickupSystem.h"
#include "PickupConfig.h"
#include "Sweets/SweetsTypes.h"

namespace SweetsSpace
{
    class BaseSweets;
    class SweetsManager;
    struct PickupQueryResult;
}

namespace Core
{
    class Input;
}

namespace Entities
{
    class Player : public PhysicalEntity
    {
    public:
        Player(const Transform& transform,
            std::shared_ptr<Render::Model> model = nullptr,
            Systems::PhysicsMachine* physicsMachine = nullptr,
            SweetsSpace::SweetsManager* sweetsManager = nullptr,
            Core::Input* input = nullptr,
            Collision::CollisionLayer collisionLayer = Collision::CollisionLayer::Player,
            bool kinematic = false,
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
            int priority = 0,
            std::string tag = "player");
        ~Player() override;

        void OnUpdate(const Core::UpdateContext& ctx) override;
        void Draw(Render::ModelRenderer& renderer, const ::CameraSpace::ICamera& camera) override;

        void SetEffectSystem(EffectSystem* system);
        void SetSweetsManager(SweetsSpace::SweetsManager* manager);
        bool IsCarryingSweets() const;
        const std::vector<std::shared_ptr<SweetsSpace::BaseSweets>>& GetCarriedSweets() const;
        void ClearCarriedSweets();
        bool IsKinematic() const;
        void SetMoveSpeed(float speed);
        void SetMoveSpeeds(float horizontal, float vertical);
        void SetBaseMoveSpeed(float horizontal, float vertical);
        void SetSpeedMultiplier(float multiplier);
        float GetMoveSpeed() const;
        float GetHorizontalMoveSpeed() const;
        float GetVerticalMoveSpeed() const;
        void SetMinimumY(float minY);
        float GetMinimumY() const;
        bool HasMinimumY() const;
        void SetScreenBounds(float minX, float maxX, std::optional<float> maxY);
        void SetPickupBox(float width, float height);
        void SetPickupArea(float width, float height, const Stage::PickupOffset& offset);
        void SetPickupMaxDistance(float maxDistance);
        void SetPickupDepthThreshold(float depthThreshold);
        void SetPickupOriginOffset(const float3& offset);
        void SetColliderFootprint(const float3& halfExtents, const float3& centerOffset);
        void SetMaxCarry(int maxCarry);

        const SweetsSpace::PickupQueryResult& GetLastPickupResult() const noexcept;

        //プレイヤーの向き
        float GetFacingSign() const ;
        void  SetFacingSign(float sign);
 

        Transform GetRenderTransform() const override;

    private:
        friend class PlayerModel;
        friend class PlayerMovement;
        friend class PlayerPickupSystem;
        friend class PlayerPhysicsBinder;

        PlayerModel m_modelSystem;
        PlayerMovement m_movementSystem;
        PlayerPickupSystem m_pickupSystem;
        PlayerPhysicsBinder m_physicsBinder;
        Core::Input* m_input = nullptr;

        float m_facingSign;
    };
}
