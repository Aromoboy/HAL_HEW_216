#pragma once

#include "Vector.h"

#include <DirectXMath.h>

namespace CameraSpace
{
	DirectX::XMVECTOR LoadFloat3(const float3& value);
	float3 StoreFloat3(DirectX::FXMVECTOR value);
	DirectX::XMVECTOR RotateOffset(const float3& offset, float yaw, float pitch);
}
