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

namespace Entities
{
    class Player;

    class PlayerModel
    {
    public:
        void Initialize(Player& player, std::shared_ptr<Render::Model> model);
        void SyncVisualTransformFromPhysics(Player& player);
        void Draw(const Player& player, Render::ModelRenderer& renderer, const CameraSpace::ICamera& camera);
        Transform GetRenderTransform() const;

    private:
        void RefreshRenderTransform();

        VisualTransform m_visual{};
        Transform m_renderTransform{};
        std::shared_ptr<Render::Model> m_fairyModel;
        bool m_missingModelWarned{ false };
    };
}
