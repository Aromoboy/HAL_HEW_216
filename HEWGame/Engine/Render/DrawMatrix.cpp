#include "DrawMatrix.h"
#include "Defines.h"

DirectX::XMFLOAT4X4 DrawMatrix::ToGpuMatrix(const DirectX::XMMATRIX& matrix, bool transpose)
{
	DirectX::XMMATRIX mat = matrix;
	if (transpose)
	{
		mat = DirectX::XMMatrixTranspose(mat);
	}

	DirectX::XMFLOAT4X4 out{};
	DirectX::XMStoreFloat4x4(&out, mat);
	return out;
}

void DrawMatrix::CreateWorldMatrix(DirectX::XMFLOAT4X4 &a_fMat, const float3& a_fPos, const float3& a_fScale, const float3& a_fAngle, bool a_bTranspose)
{
	// 各トランスフォームの行列を作成
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(a_fPos.x, a_fPos.y, a_fPos.z); 
	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(a_fScale.x, a_fScale.y, a_fScale.z); 

	DirectX::XMMATRIX Rx = DirectX::XMMatrixRotationX(a_fAngle.x);
	DirectX::XMMATRIX Ry = DirectX::XMMatrixRotationY(a_fAngle.y);
	DirectX::XMMATRIX Rz = DirectX::XMMatrixRotationZ(a_fAngle.z);

	// 行列をかけ合わせる
	DirectX::XMMATRIX mat = S * Rx * Ry * Rz * T;
	a_fMat = ToGpuMatrix(mat, a_bTranspose);
}

void DrawMatrix::CreateWorldMatrix(DirectX::XMFLOAT4X4& a_fMat, const float3& a_fPos, const float3& a_fScale, const float3& a_fAngle, DirectX::XMMATRIX a_fBone, bool a_bTranspose)
{
	// 各トランスフォームの行列を作成
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(a_fPos.x, a_fPos.y, a_fPos.z);
	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(a_fScale.x, a_fScale.y, a_fScale.z);

	DirectX::XMMATRIX Rx = DirectX::XMMatrixRotationX(a_fAngle.x);
	DirectX::XMMATRIX Ry = DirectX::XMMatrixRotationY(a_fAngle.y);
	DirectX::XMMATRIX Rz = DirectX::XMMatrixRotationZ(a_fAngle.z);

	// 行列をかけ合わせる
	DirectX::XMMATRIX mat = S * Rx * Ry * Rz * T;

	// ボーンの動きを掛け合わせる
	mat *= a_fBone;
	a_fMat = ToGpuMatrix(mat, a_bTranspose);
}



void DrawMatrix::CreateViewMatrix(DirectX::XMFLOAT4X4& a_fMat, const float3& a_fEyePosition, const float3& a_fFocusPosition, const float3& a_fUpDirection, bool a_bTranspose)
{
        constexpr float kEpsilon = 1.0e-6f;

        DirectX::XMVECTOR eye = DirectX::XMVectorSet(a_fEyePosition.x, a_fEyePosition.y, a_fEyePosition.z, 1.0f);
        DirectX::XMVECTOR focus = DirectX::XMVectorSet(a_fFocusPosition.x, a_fFocusPosition.y, a_fFocusPosition.z, 1.0f);
        DirectX::XMVECTOR up = DirectX::XMVectorSet(a_fUpDirection.x, a_fUpDirection.y, a_fUpDirection.z, 0.0f);

        float forwardLengthSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(focus, eye)));
        if (!(forwardLengthSq > kEpsilon))
        {
                focus = DirectX::XMVectorAdd(eye, DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
        }

        float upLengthSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(up));
        if (!(upLengthSq > kEpsilon))
        {
                up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        }

        DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eye, focus, up);
        a_fMat = ToGpuMatrix(view, a_bTranspose);
}
void DrawMatrix::CreateProjectionMatrix(DirectX::XMFLOAT4X4& a_fMat, const float a_fFovAngleY, const float AspectRatio, const float NearZ, const float FarZ, bool a_bTranspose)
{
        float fov = a_fFovAngleY;
        float aspect = AspectRatio;
        float nearPlane = NearZ;
        float farPlane = FarZ;

        if (!(fov > 0.0f && fov < DirectX::XM_PI))
        {
                fov = DirectX::XM_PIDIV4;
        }

        if (!(aspect > 0.0f))
        {
                aspect = 1.0f;
        }

        if (!(nearPlane > 0.0f))
        {
                nearPlane = 0.1f;
        }

        if (!(farPlane > nearPlane))
        {
                farPlane = nearPlane + 1.0f;
        }

        DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(fov, aspect, nearPlane, farPlane);
        a_fMat = ToGpuMatrix(proj, a_bTranspose);
}
