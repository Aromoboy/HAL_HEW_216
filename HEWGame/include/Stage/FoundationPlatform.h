#pragma once

#include <memory>
#include <optional>
#include <string>

#include <DirectXMath.h>

#include "AABB.h"
#include "Entities/PhysicalEntity.h"
#include "ModelDX.h"
#include "Transform.h"
#include "VisualTransform.h"

namespace Core
{
    struct UpdateContext;
}

namespace Systems
{
    class PhysicsMachine;
}

namespace Stage
{
    class FoundationPlatform : public Entities::PhysicalEntity
    {
    public:
        FoundationPlatform(const std::string& tag,
            const Transform& transform,
            std::optional<Physics::BoxColliderComponent> boxCollider = std::nullopt,
            Systems::PhysicsMachine* physicsMachine = nullptr,
            const DirectX::XMFLOAT4& color = DirectX::XMFLOAT4{ 0.305f, 0.173f, 0.125f, 1.0f },
            Core::UpdatePhase phase = Core::UpdatePhase::PrePhysics,
            int priority = 0,
            bool createCollider = true);

        void SetAnchorTransform(const Transform& transform);
        const Transform& GetAnchorTransform() const noexcept;

        void OnUpdate(Core::UpdateContext& ctx) override;
        Transform GetRenderTransform() const override;

        Render::Model* GetRenderableModel() override;
        const Render::Model* GetRenderableModel() const override;

        void SetDebugColor(const DirectX::XMFLOAT4& color);
        const DirectX::XMFLOAT4& GetDebugColor() const noexcept;

    private:
        void LoadPlatformModel();
        void ApplyColorToModelMaterials();
        void SyncVisualTransform();
        void RefreshRenderTransform();

        Transform m_anchorTransform;
        VisualTransform m_visual{};
        Transform m_renderTransform{};
        std::shared_ptr<Render::Model> m_model;
        DirectX::XMFLOAT4 m_debugColor;
        float3 m_descriptorDimensions{ 1.0f, 1.0f, 1.0f };
        AABB m_descriptorLocalBounds;
        float m_normalizationScale = 1.0f;
        float3 m_worldScale{ 1.0f, 1.0f, 1.0f };
        float m_visualPivotOffset = 0.0f;
    };
}
