#pragma once

#include "Entities/PhysicalEntity.h"
#include "Input.h"

namespace SceneSpace
{
    class Player;
}

namespace Entities
{
    class Player : public PhysicalEntity
    {
    public:
        Player(const Transform& transform,
            std::shared_ptr<Render::Model> model = nullptr,
            std::shared_ptr<Collision::ICollider> collider = nullptr,
            Physics::Physical physical = Physics::Physical{},
            Systems::PhysicsMachine* physicsMachine = nullptr,
            Systems::ContactLiteSystem* contactSystem = nullptr,
            SceneSpace::Player* legacyPlayer = nullptr,
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
            int priority = 0);
        ~Player() override = default;

        void OnUpdate(Core::UpdateContext& ctx) override;
        void Draw(Render::ModelRenderer& renderer, const CameraSpace::ICamera& camera) override;

        SceneSpace::Player* GetLegacyPlayer() const;

    private:
        Systems::ContactLiteSystem* m_contactSystem;
        SceneSpace::Player* m_legacyPlayer;
    };
}

