#include "CameraRig.h"
// Entity の定義が完全になったので、Transform を直接参照して
// カメラの追従や回転計算がしやすくなった

#include <sstream>
#include <utility>

#include <DirectXMath.h>

#if defined(_WIN32)
#       ifndef NOMINMAX
#               define NOMINMAX
#       endif
#       include <windows.h>
#endif

#include "Entity.h"

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
// オフセットをヤー・ピッチで回転させて、
// カメラの相対位置を柔軟にコントロールするためのヘルパー
        DirectX::XMVECTOR RotateOffset(const float3& offset, float yaw, float pitch)
        {
                DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, 0.0f);
                return DirectX::XMVector3Transform(LoadFloat3(offset), rotation);
        }

        float Clamp01(float value)
        {
                if (value < 0.0f)
                {
                        return 0.0f;
                }
                if (value > 1.0f)
                {
                        return 1.0f;
                }
                return value;
        }
}

CameraRig::CameraRig(std::shared_ptr<CameraSpace::BaseCamera> camera)
        : Core::UpdateNode(Core::UpdatePhase::Camera, 0)
        , m_camera(std::move(camera))
        , m_target()
        , m_offset{ 0.0f, 0.0f, 0.0f }
        , m_yaw(0.0f)
        , m_pitch(0.0f)
        , m_followSpeed(5.0f)
        , m_warnedAboutExpiredTarget(false)
        , m_lockTranslation(false)
        , m_lockYawPitch(false)
{
}

void CameraRig::SetCamera(std::shared_ptr<CameraSpace::BaseCamera> camera)
{
        if (m_camera == camera)
        {
                return;
        }
        m_camera = std::move(camera);
}

std::shared_ptr<CameraSpace::BaseCamera> CameraRig::GetCamera() const
{
        return m_camera;
}

void CameraRig::SetOffset(const float3& offset)
{
        m_offset = offset;
}

const float3& CameraRig::GetOffset() const
{
        return m_offset;
}

void CameraRig::SetYaw(float yaw)
{
        m_yaw = yaw;
}

float CameraRig::GetYaw() const
{
        return m_yaw;
}

void CameraRig::SetPitch(float pitch)
{
        m_pitch = pitch;
}

float CameraRig::GetPitch() const
{
        return m_pitch;
}

void CameraRig::SetFollowSpeed(float followSpeed)
{
        m_followSpeed = followSpeed < 0.0f ? 0.0f : followSpeed;
}

float CameraRig::GetFollowSpeed() const
{
	return m_followSpeed;
}

void CameraRig::SetTranslationLocked(bool locked)
{
	if (m_lockTranslation == locked)
	{
		return;
	}
	m_lockTranslation = locked;
	auto target = m_target.lock();
	std::ostringstream oss;
	oss << "[CameraRig] Translation lock " << (locked ? "enabled" : "disabled")
	    << ". target=" << target.get() << " followSpeed=" << m_followSpeed << "\n";
	DebugTrace(oss.str());
}

bool CameraRig::IsTranslationLocked() const
{
	return m_lockTranslation;
}

void CameraRig::SetYawPitchLocked(bool locked)
{
	if (m_lockYawPitch == locked)
	{
		return;
	}
	m_lockYawPitch = locked;
	auto target = m_target.lock();
	std::ostringstream oss;
	oss << "[CameraRig] Yaw/Pitch lock " << (locked ? "enabled" : "disabled")
	    << ". target=" << target.get() << " followSpeed=" << m_followSpeed << "\n";
	DebugTrace(oss.str());
}

bool CameraRig::IsYawPitchLocked() const
{
        return m_lockYawPitch;
}

void CameraRig::Attach(const std::shared_ptr<SceneSpace::Entity>& target)
{
        m_target = target;
        m_warnedAboutExpiredTarget = false;

        std::ostringstream oss;
        oss << "[CameraRig] Attach called. target=" << target.get()
            << " camera=" << m_camera.get() << "\n";
        DebugTrace(oss.str());
}

void CameraRig::OnUpdate(Core::UpdateContext& ctx)
{
        if (!m_camera)
        {
                DebugTrace("[CameraRig] Update skipped because camera is null.\n");
                return;
        }

        auto target = m_target.lock();
        if (!target)
        {
                if (!m_warnedAboutExpiredTarget)
                {
                        std::ostringstream oss;
                        oss << "[CameraRig] Target expired. Freezing camera update. camera=" << m_camera.get() << "\n";
                        DebugTrace(oss.str());
                        m_warnedAboutExpiredTarget = true;
                }
                return;
        }

        m_warnedAboutExpiredTarget = false;

        const auto& targetTransform = target->GetTransform();
        const float3 targetRotation = targetTransform.rotation;

        DirectX::XMVECTOR targetPos = LoadFloat3(targetTransform.position);
        DirectX::XMVECTOR offsetVec = RotateOffset(m_offset, targetRotation.y + m_yaw, targetRotation.x + m_pitch);
        DirectX::XMVECTOR desiredPos = DirectX::XMVectorAdd(targetPos, offsetVec);

        if (!m_lockTranslation)
        {
                DirectX::XMVECTOR currentPos = LoadFloat3(m_camera->GetPosition());
                const float alpha = Clamp01(m_followSpeed * ctx.dt);
                DirectX::XMVECTOR newPos = DirectX::XMVectorLerp(currentPos, desiredPos, alpha);
                m_camera->SetPosition(StoreFloat3(newPos));
        }

        float3 newRotation
        {
                targetRotation.x + m_pitch,
                targetRotation.y + m_yaw,
                targetRotation.z
        };
        if (!m_lockYawPitch)
        {
                m_camera->SetRotation(newRotation);
        }

        m_camera->RecalculateView();
}
