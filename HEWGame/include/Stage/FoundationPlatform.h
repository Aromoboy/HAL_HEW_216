#pragma once

#include <memory>
#include <string>

#include <DirectXMath.h>

#include "PhysicalEntity.h"
#include "Transform.h"

class ModelDX;

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
            Core::UpdatePhase phase = Core::UpdatePhase::PrePhysics,
            int priority = 0);

        void SetAnchorTransform(const Transform& transform);
        const Transform& GetAnchorTransform() const noexcept;

        void OnUpdate(Core::UpdateContext& ctx) override;

        ModelDX* GetRenderableModel() override;
        const ModelDX* GetRenderableModel() const override;

        void SetDebugColor(const DirectX::XMFLOAT4& color);
        const DirectX::XMFLOAT4& GetDebugColor() const noexcept;

    private:
        void LoadPlatformModel();
        void ApplyColorToModelMaterials();

        Transform m_anchorTransform;
        std::shared_ptr<ModelDX> m_model;
        DirectX::XMFLOAT4 m_debugColor;
    };
}
