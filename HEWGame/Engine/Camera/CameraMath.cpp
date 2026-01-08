#include "CameraMath.h"

namespace CameraSpace
{
	DirectX::XMVECTOR LoadFloat3(const float3& value)
	{
		return DirectX::XMVectorSet(value.x, value.y, value.z, 0.0f);
	}

	float3 StoreFloat3(DirectX::FXMVECTOR value)
	{
		DirectX::XMFLOAT3 temp;
		DirectX::XMStoreFloat3(&temp, value);
		return float3{ temp.x, temp.y, temp.z };
	}

	DirectX::XMVECTOR RotateOffset(const float3& offset, float yaw, float pitch)
	{
		DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, 0.0f);
		return DirectX::XMVector3Transform(LoadFloat3(offset), rotation);
	}
}
