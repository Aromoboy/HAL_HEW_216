#pragma once
#include"Sweets.h"

namespace SweetsSpace
{
    class Sherbet : public BaseSweets
    {
    public:
        Sherbet(const std::string& tag,
                std::shared_ptr<ModelDX> model,
                const float3& pos,
                const float3& rot,
                const float3& scl,
                std::shared_ptr<Collision::ICollider> collider,
                Collision::CollisionLayer layer = Collision::CollisionLayer::Default,
                const float3& velocity = float3{ 0.0f, 0.0f, 0.0f },
                const float3& force = float3{ 0.0f, 0.0f, 0.0f },
                float weight = 1.0f,
                float damping = 0.98f,
                Core::UpdatePhase phase = Core::UpdatePhase::Physics,
                int priority = 0);
    };
}
