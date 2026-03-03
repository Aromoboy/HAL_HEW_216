#pragma once

#include <DirectXMath.h>

class Collision
{
public:
    struct Plane
    {
        DirectX::XMFLOAT3 normal;
        DirectX::XMFLOAT3 pos;
    };

    struct Ray
    {
        DirectX::XMFLOAT3 start;
        DirectX::XMFLOAT3 direction;
    };

    struct Triangle
    {
        DirectX::XMFLOAT3 p[3];
    };

    struct Result
    {
        bool isHit = false;
        DirectX::XMFLOAT3 dir = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 point = { 0.0f, 0.0f, 0.0f };
    };

    static Result Hit(const Plane& plane, const Ray& ray);
    static Result Hit(const Triangle& tri, const DirectX::XMFLOAT3& point);
};
