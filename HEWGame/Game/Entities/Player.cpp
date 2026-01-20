#include "Entities/Player.h"

namespace Entities
{
    Player::Player(const Transform& transform, std::shared_ptr<Render::Model> model, Systems::PhysicsMachine* physicsMachine, SweetsSpace::SweetsManager* sweetsManager, Core::Input* input, Collision::CollisionLayer collisionLayer, bool kinematic, Core::UpdatePhase phase, int priority, std::string tag)
        : PhysicalEntity(transform, std::nullopt, collisionLayer, std::move(model), physicsMachine, phase, priority, std::move(tag))
        , m_modelSystem()
        , m_movementSystem()
        , m_pickupSystem(sweetsManager)
        , m_physicsBinder(kinematic)
        , m_input(input)
        , m_facingSign(1.0f) //プレイヤーの向き
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
        m_pickupSystem.SetFacingSign(m_movementSystem.GetFacingSign());
        m_modelSystem.UpdateAnimation(*this);
        m_physicsBinder.ClampVerticalPosition(*this);
        m_physicsBinder.ClampScreenBounds(*this);
        m_modelSystem.SyncVisualTransformFromPhysics(*this);
        if (m_input)
        {
            m_pickupSystem.HandleCarryInput(*this, m_modelSystem, ctx, *m_input);
        }
        m_pickupSystem.UpdateCarriedSweets(*this);
    }

    void Player::Draw(Render::ModelRenderer& renderer, const ::CameraSpace::ICamera& camera)
    {
        m_modelSystem.Draw(*this, renderer, camera);
    }

    Transform Player::GetRenderTransform() const
    {
        return m_modelSystem.GetRenderTransform();
    }

    void Player::SetSweetsManager(SweetsSpace::SweetsManager* manager)
    {
        m_pickupSystem.SetSweetsManager(manager);
    }

    bool Player::IsCarryingSweets() const
    {
        return m_pickupSystem.IsCarryingSweets();
    }

    const std::vector<std::shared_ptr<SweetsSpace::BaseSweets>>& Player::GetCarriedSweets() const
    {
        return m_pickupSystem.GetCarriedSweets();
    }

    void Player::ClearCarriedSweets()
    {
        m_pickupSystem.ClearCarriedSweets();
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

    void Player::SetMinimumY(float minY)
    {
        m_physicsBinder.SetMinimumY(minY);
        m_physicsBinder.ClampVerticalPosition(*this);
    }

    float Player::GetMinimumY() const
    {
        return m_physicsBinder.GetMinimumY();
    }

    bool Player::HasMinimumY() const
    {
        return m_physicsBinder.HasMinimumY();
    }

    void Player::SetScreenBounds(float minX, float maxX, std::optional<float> maxY)
    {
        m_physicsBinder.SetScreenBounds(minX, maxX, maxY);
    }

    void Player::SetPickupBox(float width, float height)
    {
        m_pickupSystem.SetPickupBox(width, height);
    }

    void Player::SetPickupArea(float width, float height, const Stage::PickupOffset& offset)
    {
        m_pickupSystem.SetPickupArea(width, height, offset);
    }

    void Player::SetPickupMaxDistance(float maxDistance)
    {
        m_pickupSystem.SetPickupMaxDistance(maxDistance);
    }

    void Player::SetPickupDepthThreshold(float depthThreshold)
    {
        m_pickupSystem.SetPickupDepthThreshold(depthThreshold);
    }

    void Player::SetPickupOriginOffset(const float3& offset)
    {
        m_pickupSystem.SetPickupOriginOffset(offset);
    }

    void Player::SetColliderFootprint(const float3& halfExtents, const float3& centerOffset)
    {
        m_pickupSystem.SetColliderFootprint(halfExtents, centerOffset);
    }

    const SweetsSpace::PickupQueryResult& Player::GetLastPickupResult() const noexcept
    {
        return m_pickupSystem.GetLastPickupResult();
    }
    //プレイヤーの向き
    float Player::GetFacingSign() const
    {
        return m_facingSign;
    }
    void Player::SetFacingSign(float sign)
    {
        m_facingSign = sign;
    }
}
