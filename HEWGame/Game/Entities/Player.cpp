#include "Entities/Player.h"

#include <cmath>
#include <optional>

namespace Entities
{
    Player::Player(const Transform& transform, std::shared_ptr<Render::Model> model, Systems::PhysicsMachine* physicsMachine, Core::Input* input, Collision::CollisionLayer collisionLayer, bool kinematic, Core::UpdatePhase phase, int priority, std::string tag)
        : PhysicalEntity(transform, std::nullopt, collisionLayer, std::move(model), physicsMachine, phase, priority, std::move(tag))
        , m_modelSystem()
        , m_movementSystem()
        , m_physicsBinder(kinematic)
        , m_input(input)
    {
        m_modelSystem.Initialize(*this, m_model);
    }

    Player::~Player() = default;

    void Player::OnUpdate(const Core::UpdateContext& ctx)
    {
        m_modelSystem.SyncVisualTransformFromPhysics(*this);
        if (m_input)
        {
            m_movementSystem.Update(*this, ctx, *m_input);
        }
        m_physicsBinder.ClampScreenBounds(*this);
        m_modelSystem.SyncVisualTransformFromPhysics(*this);
    }

    void Player::Draw(Render::ModelRenderer& renderer, const ::CameraSpace::ICamera& camera)
    {
        m_modelSystem.Draw(*this, renderer, camera);
    }

    Transform Player::GetRenderTransform() const
    {
        return m_modelSystem.GetRenderTransform();
    }

    bool Player::IsKinematic() const
    {
        return m_physicsBinder.IsKinematic();
    }

    void Player::SetMoveSpeed(float speed)
    {
        m_movementSystem.SetMoveSpeed(speed);
    }

    void Player::SetMoveSpeeds(float horizontal, float vertical)
    {
        m_movementSystem.SetMoveSpeeds(horizontal, vertical);
    }

    void Player::SetBaseMoveSpeed(float horizontal, float vertical)
    {
        m_movementSystem.SetBaseMoveSpeed(horizontal, vertical);
    }

    void Player::SetSpeedMultiplier(float multiplier)
    {
        m_movementSystem.SetSpeedMultiplier(multiplier);
    }

    void Player::SetYawOffsetRadians(float yawOffsetRadians)
    {
        m_movementSystem.SetYawOffsetRadians(yawOffsetRadians);
    }

    void Player::ConfigureMovement(float accel, float brake, float turnRate, float rollResponse, float inputSmoothing)
    {
        m_movementSystem.ConfigureMovement(accel, brake, turnRate, rollResponse, inputSmoothing);
    }

    float Player::GetMoveSpeed() const
    {
        return m_movementSystem.GetMoveSpeed();
    }

    float Player::GetHorizontalMoveSpeed() const
    {
        return m_movementSystem.GetHorizontalMoveSpeed();
    }

    float Player::GetVerticalMoveSpeed() const
    {
        return m_movementSystem.GetVerticalMoveSpeed();
    }

    void Player::SetScreenBounds(float minX, float maxX, float minY, float maxY)
    {
        m_physicsBinder.SetScreenBounds(minX, maxX, minY, maxY);
    }

    void Player::RegisterWrapDeltaY(float deltaY)
    {
        if (!std::isfinite(deltaY))
        {
            return;
        }

        if (m_pendingWrapEvent)
        {
            m_pendingWrapEvent->deltaY += deltaY;
            m_pendingWrapEvent->wrappedY = true;
        }
        else
        {
            m_pendingWrapEvent = WrapEvent{ deltaY, true };
        }
    }

    std::optional<Player::WrapEvent> Player::ConsumeWrapEvent()
    {
        std::optional<WrapEvent> event = m_pendingWrapEvent;
        m_pendingWrapEvent = std::nullopt;
        return event;
    }

}
