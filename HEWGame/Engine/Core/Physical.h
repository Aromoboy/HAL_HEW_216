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

        b2BodyId bodyId{ b2_nullBodyId };
    };
}
#endif
