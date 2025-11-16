// ICollider.h

#ifndef __ICOLLIDER_H__
#define __ICOLLIDER_H__


#include "Transform.h"
#include "AABB.h"

namespace Collision
{
    class ICollider
    {
    public:
        virtual ~ICollider() = default;

        virtual  AABB GetAABB() const= 0;

        // 衝突判定を行う純粋仮想関数
        virtual bool CheckCollision(ICollider* other) = 0;

        // バウンディングボックスを更新する純粋仮想関数
        virtual bool UpdateBounds(const Transform& transform) = 0;
    };
}
#endif __ICOLLIDER_H__