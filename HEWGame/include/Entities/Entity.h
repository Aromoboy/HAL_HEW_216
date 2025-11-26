#pragma once

#include <memory>
#include <string>

#include "Core/UpdateNode.h"
#include "Transform.h"
#include "ModelRender.h"
#include "ICamera.h"

namespace Entities
{
    class Entity : public Core::UpdateNode
    {
    public:
        Entity(const Transform& transform,
            std::shared_ptr<Render::Model> model = nullptr,
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
            int priority = 0,
            std::string tag = {});
        Entity(const std::string& tag,
            const float3& pos = float3{ 0.0f, 0.0f, 0.0f },
            const float3& rot = float3{ 0.0f, 0.0f, 0.0f },
            const float3& scl = float3{ 1.0f, 1.0f, 1.0f },
            Core::UpdatePhase phase = Core::UpdatePhase::Gameplay,
            int priority = 0);
        ~Entity() override = default;

        void OnUpdate(Core::UpdateContext& ctx) override;
        virtual void Draw(Render::ModelRenderer& renderer, const CameraSpace::ICamera& camera);

        Transform& GetTransform();
        const Transform& GetTransform() const;

        std::shared_ptr<Render::Model> GetModel() const;
        const std::string& GetTag() const;
        virtual Render::Model* GetRenderableModel();
        virtual const Render::Model* GetRenderableModel() const;
        bool IsRenderable() const;

    protected:
        Transform m_transform;
        std::shared_ptr<Render::Model> m_model;
        std::string m_tag;
    };
}

// Backward compatibility: allow legacy SceneSpace namespace to reference the
// new Entities implementation to avoid mismatches between old and new header
// paths.
namespace SceneSpace
{
    using Entity = Entities::Entity;
}

