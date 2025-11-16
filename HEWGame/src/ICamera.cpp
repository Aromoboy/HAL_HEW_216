#include "ICamera.h"

ICamera::ICamera(float3 a_fPos, float a_fFovy)
	: m_pos(a_fPos.x, a_fPos.y, a_fPos.z), m_look(0.0f, 0.0f, 0.0f), m_up(0.0f, 1.0f, 0.0f)
	, m_fovy(a_fFovy), m_aspect(16.0f / 9.0f)
	, m_near(CMETER(30.0f)), m_far(METER(1000.0f))
{}

DirectX::XMFLOAT4X4 ICamera::GetViewMatrix(bool transpose)
{
	DirectX::XMFLOAT4X4 mat;

	DrawMatrix::CreateViewMatrix(mat,
		float3{ m_pos.x, m_pos.y, m_pos.z },
		float3{ m_look.x, m_look.y, m_look.z },
		float3{ m_up.x, m_up.y, m_up.z },
		transpose);

	return mat;
}

DirectX::XMFLOAT4X4 ICamera::GetProjectionMatrix(bool transpose)
{
	DirectX::XMFLOAT4X4 mat;

        DrawMatrix::CreateProjectionMatrix(mat, m_fovy, m_aspect, m_near, m_far, transpose);

        return mat;
}
