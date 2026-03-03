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

    // Stability note: avoid normalizing near-zero cross vectors.
    // Point-on-edge and tiny triangles can produce zero-length vectors,
    // and XMVector3Normalize() on them can generate non-finite values.
    XMVECTOR vPoint = XMLoadFloat3(&point);

    XMVECTOR vCross[3] = {};
    float crossLen[3] = {};
    for (int i = 0; i < 3; ++i)
    {
        const XMFLOAT3& p0 = tri.p[i];
        const XMFLOAT3& p1 = tri.p[(i + 1) % 3];

        XMVECTOR vP0 = XMLoadFloat3(&p0);
        XMVECTOR vP1 = XMLoadFloat3(&p1);

        XMVECTOR vEdge = XMVectorSubtract(vP1, vP0);
        XMVECTOR vToPt = XMVectorSubtract(vPoint, vP0);

        vCross[i] = XMVector3Cross(vEdge, vToPt);
        XMStoreFloat(&crossLen[i], XMVector3Length(vCross[i]));
    }

    float refLen = 0.0f;
    XMVECTOR refCross = XMVectorZero();
    for (int i = 0; i < 3; ++i)
    {
        if (crossLen[i] > refLen)
        {
            refLen = crossLen[i];
            refCross = vCross[i];
        }
    }

    if (refLen <= EPS)
    {
        return result;
    }

    XMVECTOR refN = XMVector3Normalize(refCross);
    for (int i = 0; i < 3; ++i)
    {
        if (crossLen[i] <= EPS)
        {
            continue;
        }

        float d = 0.0f;
        XMStoreFloat(&d, XMVector3Dot(refN, XMVector3Normalize(vCross[i])));
        if (d < -EPS)
        {
            return result;
        }
    }

    result.isHit = true;
    result.point = point;
    XMStoreFloat3(&result.dir, refN);
    return result;
}
