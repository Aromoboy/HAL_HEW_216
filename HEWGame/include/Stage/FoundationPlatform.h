#pragma once

#include <memory>
#include <string>

#include <DirectXMath.h>

#include "Entities/PhysicalEntity.h"
#include "ModelDX.h"
#include "Transform.h"

namespace Core
{
    struct UpdateContext;
}

namespace Stage
{
    class FoundationPlatform : public SceneSpace::PhysicalEntity
    {
    public:
        FoundationPlatform(const std::string& tag,
            const Transform& transform,
            std::shared_ptr<Collision::ICollider> collider,
            const DirectX::XMFLOAT4& color = DirectX::XMFLOAT4{ 0.305f, 0.173f, 0.125f, 1.0f },
            Core::UpdatePhase phase = Core::UpdatePhase::PrePhysics,
            int priority = 0);

        void SetAnchorTransform(const Transform& transform);
        const Transform& GetAnchorTransform() const noexcept;

        void OnUpdate(Core::UpdateContext& ctx) override;

        Render::Model* GetRenderableModel() override;
        const Render::Model* GetRenderableModel() const override;

        void SetDebugColor(const DirectX::XMFLOAT4& color);
        const DirectX::XMFLOAT4& GetDebugColor() const noexcept;

    private:
        void LoadPlatformModel();
        void ApplyColorToModelMaterials();

        Transform m_anchorTransform;
        std::shared_ptr<Render::Model> m_model;
        DirectX::XMFLOAT4 m_debugColor;
    };
}
