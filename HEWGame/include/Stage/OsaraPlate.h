#pragma once

#include <memory>
#include <string>

#include <DirectXMath.h>

#include "Entities/PhysicalEntity.h"
#include "Transform.h"

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
    class OsaraPlate : public Entities::PhysicalEntity
    {
    public:
        OsaraPlate(const std::string& tag,
            const Transform& anchor,
            Systems::PhysicsMachine* physicsMachine = nullptr,
            Core::UpdatePhase phase = Core::UpdatePhase::PrePhysics,
            int priority = 0);

        void SetAnchorTransform(const Transform& transform);
        const Transform& GetAnchorTransform() const noexcept;

        void OnUpdate(Core::UpdateContext& ctx) override;
        Transform GetRenderTransform() const override;

        Render::Model* GetRenderableModel() override;
        const Render::Model* GetRenderableModel() const override;

    private:
        void LoadPlateModel();
        void SyncVisualTransform();
        void RebuildCollider();
        void RefreshRenderTransform();

        Transform m_anchorTransform{};
        Transform m_renderTransform{};
        std::shared_ptr<Render::Model> m_model;
        float m_normalizationScale = 1.0f;
        float3 m_worldScale{ 1.0f, 1.0f, 1.0f };
        float m_visualPivotOffset = 0.0f;
    };
}

