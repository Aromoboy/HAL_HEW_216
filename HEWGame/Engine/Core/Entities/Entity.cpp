#include "Entities/Entity.h"

namespace Entities
{
    Entity::Entity(const Transform& transform, std::shared_ptr<Render::Model> model, Core::UpdatePhase phase, int priority, std::string tag)
        : Core::UpdateNode(phase, priority)
        , m_animation(nullptr)
        , m_transform(transform)
        , m_model(std::move(model))
        , m_tag(std::move(tag))
    {
    }

    Entity::Entity(const std::string& tag, const float3& pos, const float3& rot, const float3& scl, Core::UpdatePhase phase, int priority)
        : Entity(Transform{ pos, rot, scl }, nullptr, phase, priority, tag)
    {
    }

    void Entity::OnUpdate(const Core::UpdateContext& ctx)
    {
        (void)ctx;
    }

    void Entity::Draw(Render::ModelRenderer& renderer, const ::CameraSpace::ICamera& camera)
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

    Transform Entity::GetRenderTransform() const
    {
        return m_transform;
    }

    std::shared_ptr<Render::Model> Entity::GetModel() const
    {
        return m_model;
    }

    const std::string& Entity::GetTag() const
    {
        return m_tag;
    }

    Render::Model* Entity::GetRenderableModel()
    {
        return m_model.get();
    }

    Animation* Entity::GetAnimation()
    {
        return m_animation.get();
    }

    const Render::Model* Entity::GetRenderableModel() const
    {
        return m_model.get();
    }

    bool Entity::IsRenderable() const
    {
        return m_model != nullptr;
    }
}

