#pragma once

#include <DirectXMath.h>

#include "Collision.h"

class Block
{
public:
    struct BoxCollision
    {
        DirectX::XMFLOAT3 size = { 1.0f, 1.0f, 1.0f };
    };

public:
    explicit Block(const DirectX::XMFLOAT3& size);

    void SetPos(const DirectX::XMFLOAT3& p);
    void SetRotate(float x, float y, float z);

    const DirectX::XMFLOAT3& GetPos() const;
    const DirectX::XMFLOAT3& GetRotate() const;
    const DirectX::XMFLOAT3& GetSize() const;

    Collision::Plane GetPlane() const;
    Collision::Triangle GetTriangle(int index) const;

    void Draw() const;

private:
    BoxCollision m_collision;
    DirectX::XMFLOAT3 m_pos = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 m_rotate = { 0.0f, 0.0f, 0.0f };
};
