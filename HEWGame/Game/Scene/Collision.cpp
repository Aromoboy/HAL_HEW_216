#include "Collision.h"

#include <cmath>

using namespace DirectX;

Collision::Result Collision::Hit(const Plane& plane, const Ray& ray)
{
    Result result = {};
    constexpr float EPS = 1e-6f;

    XMVECTOR vRayStart = XMLoadFloat3(&ray.start);
    XMVECTOR vRayVec = XMLoadFloat3(&ray.direction);
    XMVECTOR vRayEnd = XMVectorAdd(vRayStart, vRayVec);

    XMVECTOR vPlaneN = XMLoadFloat3(&plane.normal);
    XMVECTOR vPlanePos = XMLoadFloat3(&plane.pos);
    vPlaneN = XMVector3Normalize(vPlaneN);

    float P1 = 0.0f;
    XMVECTOR vToStart = XMVectorSubtract(vRayStart, vPlanePos);
    XMStoreFloat(&P1, XMVector3Dot(vToStart, vPlaneN));

    float P2 = 0.0f;
    XMVECTOR vToEnd = XMVectorSubtract(vRayEnd, vPlanePos);
    XMStoreFloat(&P2, XMVector3Dot(vToEnd, vPlaneN));

    if (P1 > -EPS && P2 < EPS)
    {
        float denom = std::fabs(P1) + std::fabs(P2);
        if (denom < EPS)
        {
            return result;
        }

        float rate = std::fabs(P1) / denom;
        XMVECTOR vHit = XMVectorAdd(vRayStart, XMVectorScale(vRayVec, rate));
        XMStoreFloat3(&result.point, vHit);

        result.isHit = true;
        result.dir = plane.normal;
    }

    return result;
}

Collision::Result Collision::Hit(const Triangle& tri, const XMFLOAT3& point)
{
    Result result = { false };
    constexpr float EPS = 1e-6f;

    XMVECTOR vPoint = XMLoadFloat3(&point);

    XMVECTOR vCross[3] = {};
    for (int i = 0; i < 3; ++i)
    {
        const XMFLOAT3& p0 = tri.p[i];
        const XMFLOAT3& p1 = tri.p[(i + 1) % 3];

        XMVECTOR vP0 = XMLoadFloat3(&p0);
        XMVECTOR vP1 = XMLoadFloat3(&p1);

        XMVECTOR vEdge = XMVectorSubtract(vP1, vP0);
        XMVECTOR vToPt = XMVectorSubtract(vPoint, vP0);

        vCross[i] = XMVector3Cross(vEdge, vToPt);
        vCross[i] = XMVector3Normalize(vCross[i]);
    }

    float m01 = 0.0f;
    float m12 = 0.0f;
    float m20 = 0.0f;
    XMStoreFloat(&m01, XMVector3Dot(vCross[0], vCross[1]));
    XMStoreFloat(&m12, XMVector3Dot(vCross[1], vCross[2]));
    XMStoreFloat(&m20, XMVector3Dot(vCross[2], vCross[0]));

    if (m01 >= -EPS && m12 >= -EPS && m20 >= -EPS)
    {
        result.isHit = true;
        result.point = point;
        XMStoreFloat3(&result.dir, vCross[0]);
    }

    return result;
}
