#include "CameraRig.h"
#include "Entities/Entity.h"
// カメラの追従や回転計算がしやすくなった

#include <utility>

#include <DirectXMath.h>

#include "Entities/Entity.h"

namespace
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
        , m_lockTranslation(false)
        , m_lockYawPitch(false)
        , m_followPositionX(true)
        , m_followPositionY(true)
        , m_followPositionZ(true)
        , m_inheritRotation(true)
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
}

bool CameraRig::IsYawPitchLocked() const
{
        return m_lockYawPitch;
}

void CameraRig::SetFollowPositionAxes(bool followX, bool followY, bool followZ)
{
        m_followPositionX = followX;
        m_followPositionY = followY;
        m_followPositionZ = followZ;
}

bool CameraRig::IsFollowingPositionX() const
{
        return m_followPositionX;
}

bool CameraRig::IsFollowingPositionY() const
{
        return m_followPositionY;
}

bool CameraRig::IsFollowingPositionZ() const
{
        return m_followPositionZ;
}

void CameraRig::SetInheritRotation(bool inheritRotation)
{
        m_inheritRotation = inheritRotation;
}

bool CameraRig::IsInheritingRotation() const
{
        return m_inheritRotation;
}

void CameraRig::Attach(const std::shared_ptr<Entities::Entity>& target)
{
        m_target = target;
}

void CameraRig::OnUpdate(const Core::UpdateContext& ctx)
{
        if (!m_camera)
        {
                return;
        }

        auto target = m_target.lock();
        if (!target)
        {
                return;
        }

        const auto& targetTransform = target->GetTransform();
        const float3 targetRotation = m_inheritRotation ? targetTransform.rotation : float3{ 0.0f, 0.0f, 0.0f };

        DirectX::XMVECTOR targetPos = LoadFloat3(targetTransform.position);
        DirectX::XMVECTOR offsetVec = RotateOffset(m_offset, targetRotation.y + m_yaw, targetRotation.x + m_pitch);
        DirectX::XMVECTOR desiredPos = DirectX::XMVectorAdd(targetPos, offsetVec);

        if (!m_lockTranslation)
        {
                DirectX::XMVECTOR currentPos = LoadFloat3(m_camera->GetPosition());
                if (!m_followPositionX || !m_followPositionY || !m_followPositionZ)
                {
                        DirectX::XMFLOAT3 currentPosValue;
                        DirectX::XMFLOAT3 desiredPosValue;
                        DirectX::XMStoreFloat3(&currentPosValue, currentPos);
                        DirectX::XMStoreFloat3(&desiredPosValue, desiredPos);

                        if (!m_followPositionX)
                        {
                                desiredPosValue.x = currentPosValue.x;
                        }
                        if (!m_followPositionY)
                        {
                                desiredPosValue.y = currentPosValue.y;
                        }
                        if (!m_followPositionZ)
                        {
                                desiredPosValue.z = currentPosValue.z;
                        }

                        desiredPos = DirectX::XMVectorSet(desiredPosValue.x, desiredPosValue.y, desiredPosValue.z, 0.0f);
                }
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
