#pragma once

#include <memory>
#include <string>

#include "PhysicalEntity.h"
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
            Core::UpdatePhase phase = Core::UpdatePhase::PrePhysics,
            int priority = 0);

        void SetAnchorTransform(const Transform& transform);
        const Transform& GetAnchorTransform() const noexcept;

        void OnUpdate(Core::UpdateContext& ctx) override;

    private:
        Transform m_anchorTransform;
    };
}
