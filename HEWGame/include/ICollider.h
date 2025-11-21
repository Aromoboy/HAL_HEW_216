// ICollider.h

#ifndef __ICOLLIDER_H__
#define __ICOLLIDER_H__


#include "Transform.h"
#include "AABB.h"
#include "CollisionLayer.h"

namespace Collision
{
    class ICollider
    {
    public:
        virtual ~ICollider() = default;

        void SetLayer(CollisionLayer layer) { m_layer = layer; }
        CollisionLayer GetLayer() const { return m_layer; }

        virtual AABB GetAABB() const = 0;

        // Փ˔sz֐
        virtual bool CheckCollision(const Collision::ICollider& other) = 0;

        // oEfBO{bNXXV鏃z֐
        virtual void UpdateBounds(const Transform& transform) = 0;

    protected:
        CollisionLayer m_layer{ CollisionLayer::Default };
    };
}
#endif __ICOLLIDER_H__
