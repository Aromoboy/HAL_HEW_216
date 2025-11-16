#include "DrawMatrix.h"
#include "Defines.h"

#include <sstream>
#include <string>

#if defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
#endif

namespace
{
	void DebugTrace(const std::string& message)
	{
#if defined(_WIN32) && defined(_DEBUG)
		::OutputDebugStringA(message.c_str());
#else
		(void)message;
#endif
	}
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
	// GPU用に配列を変換
	if (a_bTranspose) mat = DirectX::XMMatrixTranspose(mat);

	// 描画用の配列を代入
	DirectX::XMStoreFloat4x4(&a_fMat, mat);
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

	// Boneの動きを掛け合わせる
	mat *= a_fBone;
	// GPU用に配列を変換
	if (a_bTranspose) mat = DirectX::XMMatrixTranspose(mat);

	// 描画用の配列を代入
	DirectX::XMStoreFloat4x4(&a_fMat, mat);
}



void DrawMatrix::CreateViewMatrix(DirectX::XMFLOAT4X4& a_fMat, const float3& a_fEyePosition, const float3& a_fFocusPosition, const float3& a_fUpDirection, bool a_bTranspose)
{
	DirectX::XMMATRIX view;

	float aspect = AspectRatio;
	float nearPlane = NearZ;
	float farPlane = FarZ;

	if (!(aspect > 0.0f))
	{
#if defined(_DEBUG)
		std::ostringstream oss;
		oss << "[DrawMatrix] CreateProjectionMatrix invalid aspect ratio value=" << aspect
		    << " fallback=1.0\n";
		DebugTrace(oss.str());
#endif
		aspect = 1.0f;
	}

	if (!(nearPlane > 0.0f))
	{
#if defined(_DEBUG)
		std::ostringstream oss;
		oss << "[DrawMatrix] CreateProjectionMatrix invalid near plane value=" << nearPlane
		    << " fallback=0.1\n";
		DebugTrace(oss.str());
#endif
		nearPlane = 0.1f;
	}

	if (!(farPlane > nearPlane))
	{
#if defined(_DEBUG)
		std::ostringstream oss;
		oss << "[DrawMatrix] CreateProjectionMatrix invalid far plane value=" << farPlane
		    << " (must be greater than near plane=" << nearPlane << ") fallback=" << (nearPlane + 1.0f) << "\n";
		DebugTrace(oss.str());
#endif
		farPlane = nearPlane + 1.0f;
	}

	// vWFNVWւ̕ϊs
		a_fFovAngleY,
		aspect,
		nearPlane,
		farPlane

	if (a_bTranspose) proj = DirectX::XMMatrixTranspose(proj);



	// GPU用の行列に変換
	DirectX::XMStoreFloat4x4(&a_fMat, view);
}

void DrawMatrix::CreateProjectionMatrix(DirectX::XMFLOAT4X4& a_fMat, const float a_fFovAngleY, const float AspectRatio, const float NearZ, const float FarZ, bool a_bTranspose)
{
	DirectX::XMMATRIX proj;

	// プロジェクション座標への変換行列
	proj = DirectX::XMMatrixPerspectiveFovLH(
		60.0f * RAD,	// y軸の視野角
		16 / 9,	// y軸の視野角からx軸の視野角を割り出すための割合
		0.1f,	// カメラの最小表示距離
		100.0f	// カメラの最大表示距離
	);

	if(a_bTranspose) proj = DirectX::XMMatrixTranspose(proj);

	DirectX::XMStoreFloat4x4(&a_fMat, proj);
}
