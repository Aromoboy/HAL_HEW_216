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
        void Initialize(Player& player, std::shared_ptr<Render::Model> model);
        void SyncVisualTransformFromPhysics(Player& player);
        void UpdateAnimation(Player& player);
        void Draw(const Player& player, Render::ModelRenderer& renderer, const CameraSpace::ICamera& camera);
        void PlayStabAnimation(Player& player);
        Transform GetRenderTransform() const;
        std::shared_ptr<Animation> GetActiveAnimation() const;

    private:
        void RefreshRenderTransform();

        VisualTransform m_visual{};
        Transform m_renderTransform{};
        std::shared_ptr<Render::Model> m_fairyModel;
        std::shared_ptr<Animation> m_activeAnimation;
        std::shared_ptr<Animation> m_idleAnimation;
        std::shared_ptr<Animation> m_stabAnimation;
        bool m_missingModelWarned{ false };
    };
}
