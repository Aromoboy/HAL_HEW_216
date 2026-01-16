// プレイヤー本体の動きと持ち上げをまとめたクラスだ
// 入力に合わせて移動や向きが変わり拾ったスイーツの状態も変える
// 取得した判定結果はスイーツ側やゲーム進行の判断に使われる
#pragma once

#include <memory>
#include <optional>
#include <string>

#include "Entities/PhysicalEntity.h"
#include "Entities/PlayerModel.h"
#include "Entities/PlayerMovement.h"
#include "Entities/PlayerPhysicsBinder.h"

namespace Core
{
    class Input;
}

namespace Entities
{
    class Player : public PhysicalEntity
    {
    public:
        struct WrapEvent
        {
            float deltaY = 0.0f;
            bool wrappedY = false;
        };

        Player(const Transform& transform,
            std::shared_ptr<Render::Model> model = nullptr,
            Systems::PhysicsMachine* physicsMachine = nullptr,
            Core::Input* input = nullptr,
            Collision::CollisionLayer collisionLayer = Collision::CollisionLayer::Player,
            bool kinematic = false,
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
            int priority = 0,
            std::string tag = "player");
        ~Player() override;

        void OnUpdate(const Core::UpdateContext& ctx) override;
        void Draw(Render::ModelRenderer& renderer, const ::CameraSpace::ICamera& camera) override;

        bool IsKinematic() const;
        void SetMoveSpeed(float speed);
        void SetMoveSpeeds(float horizontal, float vertical);
        void SetBaseMoveSpeed(float horizontal, float vertical);
        void SetSpeedMultiplier(float multiplier);
        void SetYawOffsetRadians(float yawOffsetRadians);
        void ConfigureMovement(float accel, float brake, float turnRate, float rollResponse, float inputSmoothing, float maxSpeed);
        float GetMoveSpeed() const;
        float GetHorizontalMoveSpeed() const;
        float GetVerticalMoveSpeed() const;
        void SetScreenBounds(float minX, float maxX, float minY, float maxY);
        void SetWrapMargin(float margin);
        void SetWrapEnabled(bool wrapX, bool wrapY);
        void SetRenderAlpha(float alpha);
        float GetRenderAlpha() const;
        void SetInvulnerable(bool invulnerable);
        bool IsInvulnerable() const;
        std::optional<WrapEvent> ConsumeWrapEvent();
        Transform GetRenderTransform() const override;

    private:
        friend class PlayerModel;
        friend class PlayerMovement;
        friend class PlayerPhysicsBinder;

        void RegisterWrapDeltaY(float deltaY);

        PlayerModel m_modelSystem;
        PlayerMovement m_movementSystem;
        PlayerPhysicsBinder m_physicsBinder;
        Core::Input* m_input = nullptr;
        std::optional<WrapEvent> m_pendingWrapEvent = std::nullopt;
        float m_renderAlpha = 1.0f;
        bool m_invulnerable = false;
    };
}
