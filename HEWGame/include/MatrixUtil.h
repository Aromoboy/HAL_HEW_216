#pragma once

#include <DirectXMath.h>

#include "Vector.h"

namespace MathUtil
{
    inline DirectX::XMFLOAT4X4 MakeWorldMatrix(
        const float3& pos,
        const float3& scale,
        const float3& radians,
        bool transpose = true)
    {
        DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);
        DirectX::XMMATRIX scaling = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);

        DirectX::XMMATRIX rotationX = DirectX::XMMatrixRotationX(radians.x);
        DirectX::XMMATRIX rotationY = DirectX::XMMatrixRotationY(radians.y);
        DirectX::XMMATRIX rotationZ = DirectX::XMMatrixRotationZ(radians.z);

        DirectX::XMMATRIX world = scaling * rotationX * rotationY * rotationZ * translation;
        if (transpose)
        {
            world = DirectX::XMMatrixTranspose(world);
        }

        DirectX::XMFLOAT4X4 result{};
        DirectX::XMStoreFloat4x4(&result, world);
        return result;
    }

    inline DirectX::XMFLOAT4X4 MakeViewMatrix(
        const float3& eyePosition,
        const float3& focusPosition,
        const float3& upDirection,
        bool transpose = true)
    {
        constexpr float kEpsilon = 1.0e-6f;

        DirectX::XMVECTOR eye = DirectX::XMVectorSet(eyePosition.x, eyePosition.y, eyePosition.z, 1.0f);
        DirectX::XMVECTOR focus = DirectX::XMVectorSet(focusPosition.x, focusPosition.y, focusPosition.z, 1.0f);
        DirectX::XMVECTOR up = DirectX::XMVectorSet(upDirection.x, upDirection.y, upDirection.z, 0.0f);

        const float forwardLengthSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(focus, eye)));
        if (!(forwardLengthSq > kEpsilon))
        {
            focus = DirectX::XMVectorAdd(eye, DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
        }

        const float upLengthSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(up));
        if (!(upLengthSq > kEpsilon))
        {
            up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        }

        DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eye, focus, up);
        if (transpose)
        {
            view = DirectX::XMMatrixTranspose(view);
        }

        DirectX::XMFLOAT4X4 result{};
        DirectX::XMStoreFloat4x4(&result, view);
        return result;
    }

    inline DirectX::XMFLOAT4X4 MakeProjectionMatrix(
        float fovY,
        float aspectRatio,
        float nearPlane,
        float farPlane,
        bool transpose = true)
    {
        if (!(fovY > 0.0f && fovY < DirectX::XM_PI))
        {
            fovY = DirectX::XM_PIDIV4;
        }

        if (!(aspectRatio > 0.0f))
        {
            aspectRatio = 1.0f;
        }

        if (!(nearPlane > 0.0f))
        {
            nearPlane = 0.1f;
        }

        if (!(farPlane > nearPlane))
        {
            farPlane = nearPlane + 1.0f;
        }

        DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(fovY, aspectRatio, nearPlane, farPlane);
        if (transpose)
        {
            proj = DirectX::XMMatrixTranspose(proj);
        }

        DirectX::XMFLOAT4X4 result{};
        DirectX::XMStoreFloat4x4(&result, proj);
        return result;
    }
}

