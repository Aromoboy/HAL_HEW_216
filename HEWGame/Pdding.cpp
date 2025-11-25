#include "Pdding.h"

namespace SweetsSpace
{
    Pudding::Pudding(const std::string& tag,
                     std::shared_ptr<ModelDX> model,
                     const float3& pos,
                     const float3& rot,
                     const float3& scl,
                     std::shared_ptr<Collision::ICollider> collider,
                     Collision::CollisionLayer layer,
                     const float3& velocity,
                     const float3& force,
                     float weight,
                     float damping,
                     Core::UpdatePhase phase,
                     int priority)
        : LegacyBaseSweets(tag, model, pos, rot, scl, collider, layer, velocity, force,
          weight, damping, phase, priority)
    {}
}
