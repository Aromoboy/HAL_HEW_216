#pragma once

#include "DirectXMath.h"
#include "Geometory.h"
#include "Vector.h"


class DrawMatrix
{
public:
	static DirectX::XMFLOAT4X4 ToGpuMatrix(const DirectX::XMMATRIX& matrix, bool transpose = true);

	static void CreateWorldMatrix(DirectX::XMFLOAT4X4& a_fMat, const float3& a_fPos, const float3& a_fScale, const float3& a_fAngle, bool a_bTranspose = true);
	static void CreateWorldMatrix(DirectX::XMFLOAT4X4& a_fMat, const float3& a_fPos, const float3& a_fScale, const float3& a_fAngle, DirectX::XMMATRIX a_fBone, bool a_bTranspose = true);
	static void CreateViewMatrix(DirectX::XMFLOAT4X4& a_fMat, const float3& a_fEyePosition, const float3& a_fFocusPosition, const float3& a_fUpDirection, bool a_bTranspose = true);
	static void CreateProjectionMatrix(DirectX::XMFLOAT4X4& a_fMat, const float a_fFovAngleY, const float AspectRatio, const float NearZ, const float FarZ, bool a_bTranspose = true);

};
