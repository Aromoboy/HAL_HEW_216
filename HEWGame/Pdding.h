#pragma once
#include"Sweets.h"

namespace SweetsSpace
{
    class Pudding : public BaseSweets
    {
    public:
        Pudding(const std::string& tag,
                std::shared_ptr<ModelDX> model,
                const float3& pos,
                const float3& rot,
                const float3& scl,
                std::shared_ptr<Collision::ICollider> collider,
                Collision::CollisionLayer layer = Collision::CollisionLayer::Default,
                const float3& velocity,
                const float3& force,
                float weight  = 1.0f,
                float damping = 0.98f,
                Core::UpdatePhase phase = Core::UpdatePhase::Physics,
                int priority = 0);
    };
}

