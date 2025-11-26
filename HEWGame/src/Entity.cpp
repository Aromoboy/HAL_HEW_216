// Entity.cpp
#include "Entities/Entity.h"

#include <sstream>
#include <string>

#if defined(_WIN32)
#       ifndef NOMINMAX
#               define NOMINMAX
#       endif
#       include <windows.h>
#endif

#include "ModelDX.h"

namespace
{
        void DebugTrace(const std::string& message)
        {
#if defined(_WIN32) && defined(_DEBUG)
                ::OutputDebugStringA(message.c_str());
#else
                (void)message;
#endif
        }

        constexpr const char* kLogPrefix = "[Entity] ";
}

namespace SceneSpace
{
    Entity::Entity(const std::string& tag,
        const float3& pos,
        const float3& rot,
        const float3& scl,
        Core::UpdatePhase phase,
        int priority)
        : Core::UpdateNode(phase, priority)
        , m_transform{}
        , m_tag(tag)
    {
        m_transform.position = pos;
        m_transform.rotation = rot;
        m_transform.scale = scl;
    }

    Entity::~Entity() = default;

    void Entity::OnUpdate(Core::UpdateContext& ctx)
    {
        (void)ctx;
    }

    void Entity::Draw(ModelRender render, CameraSpace::ICamera& Camera)
    {
        ModelDX* model = GetRenderableModel();
        if (!model)
        {
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << kLogPrefix << "Draw skipped: no renderable model assigned. tag='" << m_tag
                    << "' entity=" << this << "\n";
                DebugTrace(oss.str());
#endif
                return;
        }

        render.Draw(*model, m_transform, &Camera);
    }

    ModelDX* Entity::GetRenderableModel()
    {
        return nullptr;
    }

    const ModelDX* Entity::GetRenderableModel() const
    {
        return nullptr;
    }

    bool Entity::IsRenderable() const
    {
        return GetRenderableModel() != nullptr;
    }

    Transform& Entity::GetTransform()
    {
        return m_transform;
    }

    const Transform& Entity::GetTransform() const
    {
        return m_transform;
    }

    const std::string& Entity::GetTag() const
    {
        return m_tag;
    }
}
