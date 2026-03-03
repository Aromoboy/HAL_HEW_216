#include "Block.h"

#include "Geometory.h"

using namespace DirectX;

namespace
{
    XMFLOAT4X4 ToFloat4x4(const XMMATRIX& matrix)
    {
        XMFLOAT4X4 out{};
        XMStoreFloat4x4(&out, matrix);
        return out;
    }
}

Block::Block(const XMFLOAT3& size)
{
    m_collision.size = size;
}

void Block::SetPos(const XMFLOAT3& p)
{
    m_pos = p;
}

void Block::SetRotate(float x, float y, float z)
{
    m_rotate = { x, y, z };
}

const XMFLOAT3& Block::GetPos() const
{
    return m_pos;
}

const XMFLOAT3& Block::GetRotate() const
{
    return m_rotate;
}

const XMFLOAT3& Block::GetSize() const
{
    return m_collision.size;
}

Collision::Plane Block::GetPlane() const
{
    Collision::Plane plane = {};

    XMMATRIX mRotX = XMMatrixRotationX(m_rotate.x);
    XMMATRIX mRotY = XMMatrixRotationY(m_rotate.y);
    XMMATRIX mRotZ = XMMatrixRotationZ(m_rotate.z);
    XMMATRIX mRot = mRotX * mRotY * mRotZ;

    plane.normal = { 0.0f, 1.0f, 0.0f };
    plane.pos = { 0.0f, m_collision.size.y * 0.5f, 0.0f };

    XMVECTOR vN = XMLoadFloat3(&plane.normal);
    XMVECTOR vPos = XMLoadFloat3(&plane.pos);

    vN = XMVector3TransformNormal(vN, mRot);
    vPos = XMVector3TransformCoord(vPos, mRot);

    XMStoreFloat3(&plane.normal, vN);
    XMStoreFloat3(&plane.pos, vPos);

    plane.pos.x += m_pos.x;
    plane.pos.y += m_pos.y;
    plane.pos.z += m_pos.z;

    return plane;
}

Collision::Triangle Block::GetTriangle(int index) const
{
    Collision::Triangle tri = {};

    const XMFLOAT3 local[2][3] = {
        { { -0.5f, 0.5f, -0.5f }, { -0.5f, 0.5f, 0.5f }, { 0.5f, 0.5f, 0.5f } },
        { { 0.5f, 0.5f, 0.5f }, { 0.5f, 0.5f, -0.5f }, { -0.5f, 0.5f, -0.5f } }
    };

    const int clampedIndex = (index == 0) ? 0 : 1;

    XMMATRIX mScale = XMMatrixScaling(m_collision.size.x, m_collision.size.y, m_collision.size.z);
    XMMATRIX mRotX = XMMatrixRotationX(m_rotate.x);
    XMMATRIX mRotY = XMMatrixRotationY(m_rotate.y);
    XMMATRIX mRotZ = XMMatrixRotationZ(m_rotate.z);
    XMMATRIX mTrans = XMMatrixTranslation(m_pos.x, m_pos.y, m_pos.z);

    XMMATRIX mMat = mScale * mRotX * mRotY * mRotZ * mTrans;

    for (int i = 0; i < 3; ++i)
    {
        XMVECTOR v = XMLoadFloat3(&local[clampedIndex][i]);
        v = XMVector3TransformCoord(v, mMat);
        XMStoreFloat3(&tri.p[i], v);
    }

    return tri;
}

void Block::Draw() const
{
    XMMATRIX mScale = XMMatrixScaling(m_collision.size.x, m_collision.size.y, m_collision.size.z);
    XMMATRIX mRotX = XMMatrixRotationX(m_rotate.x);
    XMMATRIX mRotY = XMMatrixRotationY(m_rotate.y);
    XMMATRIX mRotZ = XMMatrixRotationZ(m_rotate.z);
    XMMATRIX mTrans = XMMatrixTranslation(m_pos.x, m_pos.y, m_pos.z);
    Geometory::SetWorld(ToFloat4x4(mScale * mRotX * mRotY * mRotZ * mTrans));
    Geometory::DrawBox();
}
