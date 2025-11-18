// ============================================================================
// 概要: DrawMatrix の挙動や関連ロジックを実装し、src に属する機能を具体化している。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: src 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

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
        constexpr float kEpsilon = 1.0e-6f;

        DirectX::XMVECTOR eye = DirectX::XMVectorSet(a_fEyePosition.x, a_fEyePosition.y, a_fEyePosition.z, 1.0f);
        DirectX::XMVECTOR focus = DirectX::XMVectorSet(a_fFocusPosition.x, a_fFocusPosition.y, a_fFocusPosition.z, 1.0f);
        DirectX::XMVECTOR up = DirectX::XMVectorSet(a_fUpDirection.x, a_fUpDirection.y, a_fUpDirection.z, 0.0f);

        float forwardLengthSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(DirectX::XMVectorSubtract(focus, eye)));
        if (!(forwardLengthSq > kEpsilon))
        {
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << "[DrawMatrix] CreateViewMatrix invalid focus vector. eye="
                        << a_fEyePosition.x << ',' << a_fEyePosition.y << ',' << a_fEyePosition.z
                        << " focus=" << a_fFocusPosition.x << ',' << a_fFocusPosition.y << ',' << a_fFocusPosition.z
                        << " fallback to +Z\n";
                DebugTrace(oss.str());
#endif
                focus = DirectX::XMVectorAdd(eye, DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
        }

        float upLengthSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(up));
        if (!(upLengthSq > kEpsilon))
        {
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << "[DrawMatrix] CreateViewMatrix invalid up vector="
                        << a_fUpDirection.x << ',' << a_fUpDirection.y << ',' << a_fUpDirection.z
                        << " fallback to world up\n";
                DebugTrace(oss.str());
#endif
                up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        }

        DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eye, focus, up);
        if (a_bTranspose)
        {
                view = DirectX::XMMatrixTranspose(view);
        }

        DirectX::XMStoreFloat4x4(&a_fMat, view);
}
void DrawMatrix::CreateProjectionMatrix(DirectX::XMFLOAT4X4& a_fMat, const float a_fFovAngleY, const float AspectRatio, const float NearZ, const float FarZ, bool a_bTranspose)
{
        float fov = a_fFovAngleY;
        float aspect = AspectRatio;
        float nearPlane = NearZ;
        float farPlane = FarZ;

        if (!(fov > 0.0f && fov < DirectX::XM_PI))
        {
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << "[DrawMatrix] CreateProjectionMatrix invalid fov value=" << fov
                        << " fallback=XM_PIDIV4\n";
                DebugTrace(oss.str());
#endif
                fov = DirectX::XM_PIDIV4;
        }

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

        DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(fov, aspect, nearPlane, farPlane);
        if (a_bTranspose)
        {
                proj = DirectX::XMMatrixTranspose(proj);
        }

        DirectX::XMStoreFloat4x4(&a_fMat, proj);
}
