#pragma once

#include <box2d/box2d.h>

namespace Systems
{
    struct ContactEventInfo
    {
        b2ShapeId shapeIdA{ b2_nullShapeId };
        b2ShapeId shapeIdB{ b2_nullShapeId };
        b2ContactId contactId{ b2_nullContactId };
    };
}
