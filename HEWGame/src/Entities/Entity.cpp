#include "Entities/Entity.h"

namespace Entities
{
    Entity::Entity(const Transform& transform, std::shared_ptr<Render::Model> model, Core::UpdatePhase phase, int priority)
        : Core::UpdateNode(phase, priority)
        , m_transform(transform)
        , m_model(std::move(model))
    {
    }

    void Entity::OnUpdate(Core::UpdateContext& ctx)
    {
        (void)ctx;
    }

    void Entity::Draw(Render::ModelRenderer& renderer, const CameraSpace::ICamera& camera)
    {
        (void)renderer;
        (void)camera;
    }

    Transform& Entity::GetTransform()
    {
        return m_transform;
    }

    const Transform& Entity::GetTransform() const
    {
        return m_transform;
    }

    std::shared_ptr<Render::Model> Entity::GetModel() const
    {
        return m_model;
    }
}

