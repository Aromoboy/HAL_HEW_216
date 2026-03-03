#pragma once

#include <DirectXMath.h>

class Ball
{
public:
    DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 vel = { 0.0f, 0.0f, 0.0f };
    float radius = 0.5f;

    void Reset(const DirectX::XMFLOAT3& p)
    {
        pos = p;
        vel = { 0.0f, 0.0f, 0.0f };
    }
};
