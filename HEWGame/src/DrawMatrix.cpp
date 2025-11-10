#include "DrawMatrix.h"
#include "Defines.h"

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

	// ビュー座標への変換行列
	view = DirectX::XMMatrixLookAtLH(
		DirectX::XMVectorSet(a_fEyePosition.x, a_fEyePosition.y, a_fEyePosition.z, 0.0f),  // カメラの位置
		DirectX::XMVectorSet(a_fFocusPosition.x, a_fFocusPosition.y, a_fFocusPosition.z, 0.0f),  // カメラの注視点
		DirectX::XMVectorSet(a_fUpDirection.x, a_fUpDirection.y, a_fUpDirection.z, 0.0f)); // カメラにとっての上方向

	if (a_bTranspose) view = DirectX::XMMatrixTranspose(view);


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
