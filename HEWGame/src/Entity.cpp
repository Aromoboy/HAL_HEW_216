#include "Entity.h"

#include "ModelRender.h"

#include <utility>

namespace SceneSpace
{
        Entity::Entity(std::string tag, const float3& pos, const float3& rot, const float3& scl)
                : m_transform{}
                , m_tag(std::move(tag))
        {
                m_transform.position = pos;
                m_transform.rotation = rot;
                m_transform.scale = scl;
        }

        Entity::~Entity() = default;

        void Entity::OnUpdate()
        {
        }

        void Entity::Draw(ModelRender& renderer)
        {
                (void)renderer;
        }

        Transform& Entity::GetTransform()
        {
                return m_transform;
        }

        const Transform& Entity::GetTransform() const
        {
                return m_transform;
        }
}
