#include "Sherbet.h"

namespace SweetsSpace
{
    Sherbet::Sherbet(const std::string& tag,
                     std::shared_ptr<ModelDX> model,
                     const float3& pos,
                     const float3& rot,
                     const float3& scl,
                     std::shared_ptr<Collision::ICollider> collider,
                     const float3& velocity,
                     const float3& force,
                     float weight,
                     float damping,
                     Core::UpdatePhase phase,
                     int priority)
        : BaseSweets(tag, model, pos, rot, scl, collider, velocity, force,
          weight, damping, phase, priority)
    { }
}
