#pragma once

#include <DirectXMath.h>

namespace CameraSpace
{
	class ICamera
	{
	public:
		virtual ~ICamera() = default;

		virtual const DirectX::XMFLOAT4X4& GetView() const = 0;
		virtual const DirectX::XMFLOAT4X4& GetProjection() const = 0;

	protected:
		virtual void RecalculateView() = 0;
	};
}
