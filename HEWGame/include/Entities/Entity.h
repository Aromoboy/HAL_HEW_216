#pragma once

#include <memory>
#include <string>

#include "Core/UpdateNode.h"
#include "Transform.h"
#include "ModelRender.h"
#include "ICamera.h"

class ModelDX;
namespace Render
{
    using Model = ::ModelDX;
}

namespace Entities
{
    class Entity : public Core::UpdateNode
    {
    public:
        Entity(const Transform& transform,
            std::shared_ptr<Render::Model> model = nullptr,
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
            int priority = 0);
        ~Entity() override = default;

        void OnUpdate(Core::UpdateContext& ctx) override;
        virtual void Draw(Render::ModelRenderer& renderer, const CameraSpace::ICamera& camera);

        Transform& GetTransform();
        const Transform& GetTransform() const;

        std::shared_ptr<Render::Model> GetModel() const;

    protected:
        Transform m_transform;
        std::shared_ptr<Render::Model> m_model;
    };
}

