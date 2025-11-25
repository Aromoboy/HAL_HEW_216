#include "Entities/Player.h"

#include "Player.h"

namespace Entities
{
    Player::Player(const Transform& transform, std::shared_ptr<Render::Model> model, std::shared_ptr<Collision::ICollider> collider, Physics::Physical physical, Systems::PhysicsMachine* physicsMachine, Systems::ContactLiteSystem* contactSystem, SceneSpace::Player* legacyPlayer, Core::UpdatePhase phase, int priority)
        : PhysicalEntity(transform, std::move(collider), Collision::CollisionLayer::Default, physical, std::move(model), physicsMachine, contactSystem, phase, priority)
        , m_contactSystem(contactSystem)
        , m_legacyPlayer(legacyPlayer)
    {
    }

    void Player::OnUpdate(Core::UpdateContext& ctx)
    {
        if (m_legacyPlayer)
        {
            m_legacyPlayer->OnUpdate(ctx);
        }
        (void)ctx;
    }

    void Player::Draw(Render::ModelRenderer& renderer, const CameraSpace::ICamera& camera)
    {
        if (m_legacyPlayer)
        {
            m_legacyPlayer->Draw(renderer, const_cast<CameraSpace::ICamera&>(camera));
        }
        (void)renderer;
        (void)camera;
    }

    SceneSpace::Player* Player::GetLegacyPlayer() const
    {
        return m_legacyPlayer;
    }
}

