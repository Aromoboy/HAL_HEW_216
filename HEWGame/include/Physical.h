// Physical.h
#ifndef __PHYSICAL_H__
#define __PHYSICAL_H__

#include <box2d/box2d.h>

#include "Vector.h"

namespace Physics
{
    struct Physical
    {
        Physical() = default;
        explicit Physical(b2BodyId body)
            : bodyId(body)
        {
        }

        [[nodiscard]] bool HasBody() const
        {
            return b2Body_IsValid(bodyId);
        }

        void Reset()
        {
            bodyId = b2_nullBodyId;
        }

        [[nodiscard]] b2BodyType GetBodyType() const
        {
            return HasBody() ? b2Body_GetType(bodyId) : b2_staticBody;
        }

        [[nodiscard]] bool IsDynamic() const
        {
            return GetBodyType() == b2_dynamicBody;
        }

        [[nodiscard]] float3 GetLinearVelocity() const
        {
            if (!HasBody())
            {
                return float3{ 0.0f, 0.0f, 0.0f };
            }

            const b2Vec2 v = b2Body_GetLinearVelocity(bodyId);
            return float3{ v.x, v.y, 0.0f };
        }

        void SetLinearVelocity(const float3& velocity)
        {
            if (!HasBody())
            {
                return;
            }

            b2Body_SetLinearVelocity(bodyId, b2Vec2{ velocity.x, velocity.y });
        }

        [[nodiscard]] float GetLinearDamping() const
        {
            return HasBody() ? b2Body_GetLinearDamping(bodyId) : 0.0f;
        }

        void SetLinearDamping(float damping)
        {
            if (HasBody())
            {
                b2Body_SetLinearDamping(bodyId, damping);
            }
        }

        [[nodiscard]] float GetGravityScale() const
        {
            return HasBody() ? b2Body_GetGravityScale(bodyId) : 0.0f;
        }

        void SetGravityScale(float scale)
        {
            if (HasBody())
            {
                b2Body_SetGravityScale(bodyId, scale);
            }
        }

        b2BodyId bodyId{ b2_nullBodyId };
    };
}
#endif
