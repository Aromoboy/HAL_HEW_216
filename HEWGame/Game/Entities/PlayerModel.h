#pragma once

#include <memory>

#include "ModelRender.h"
#include "Render/Model.h"
#include "Transform.h"
#include "VisualTransform.h"

namespace CameraSpace
{
    class ICamera;
}

class Animation;

namespace Entities
{
    class Player;

    class PlayerModel
    {
    public:
        enum PlayerAnimState
        {
            IDLE,
            MOVE,
            STAB,
            CARRY
        };

        void Initialize(Player& player, std::shared_ptr<Render::Model> model);
        void SyncVisualTransformFromPhysics(Player& player);
        void UpdateAnimation(Player& player);
        void Draw(const Player& player, Render::ModelRenderer& renderer, const CameraSpace::ICamera& camera);
        void PlayIdle(Player& player);
        void PlayStabAnimation(Player& player);
        void PlayMove(Player& player);
        void PlayUnCarry(Player& player);
        Transform GetRenderTransform() const;
        std::shared_ptr<Animation> GetActiveAnimation() const;
        bool IsActionAnimation();

    private:
        void RefreshRenderTransform();

        VisualTransform m_visual{};
        Transform m_renderTransform{};
        std::shared_ptr<Render::Model> m_fairyModel;
        std::shared_ptr<Animation> m_activeAnimation;
        PlayerAnimState m_activAnimationState;
        PlayerAnimState m_dontActionAnimationState;
        std::shared_ptr<Animation> m_idleAnimation;
        std::shared_ptr<Animation> m_stabAnimation;
        std::shared_ptr<Animation> m_moveAnimation;
        bool m_missingModelWarned{ false };
    };
}
