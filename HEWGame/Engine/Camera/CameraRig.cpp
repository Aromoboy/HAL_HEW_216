#include "CameraRig.h"
#include "CameraMath.h"
#include "Entities/Entity.h"
// カメラの追従や回転計算がしやすくなった

#include <cmath>
#include <utility>

#include <DirectXMath.h>

namespace
{
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
        , m_targetRaw(nullptr)
        , m_offset{ 0.0f, 0.0f, 0.0f }
        , m_yaw(0.0f)
        , m_pitch(0.0f)
        , m_followSpeed(5.0f)
        , m_followSpeedY(5.0f)
        , m_comfortZoneTopY(0.0f)
        , m_comfortZoneBottomY(0.0f)
        , m_lockTranslation(false)
        , m_lockYawPitch(false)
        , m_followPositionX(true)
        , m_followPositionY(true)
        , m_followPositionZ(true)
        , m_inheritRotation(true)
        , m_minYClamp(std::nullopt)
        , m_maxYClamp(std::nullopt)
        , m_minYWrap(std::nullopt)
        , m_maxYWrap(std::nullopt)
        , m_scrollY(0.0f)
        , m_hasScrollY(false)
        , m_lastTargetYRaw(std::nullopt)
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
        const float clampedSpeed = followSpeed < 0.0f ? 0.0f : followSpeed;
        const float previousSpeed = m_followSpeed;
        m_followSpeed = clampedSpeed;
        if (m_followSpeedY == previousSpeed)
        {
                m_followSpeedY = clampedSpeed;
        }
}

float CameraRig::GetFollowSpeed() const
{
	return m_followSpeed;
}

void CameraRig::SetFollowSpeedY(float followSpeed)
{
        m_followSpeedY = followSpeed < 0.0f ? 0.0f : followSpeed;
}

float CameraRig::GetFollowSpeedY() const
{
        return m_followSpeedY;
}

void CameraRig::SetComfortZoneY(float comfortTopY, float comfortBottomY)
{
        m_comfortZoneTopY = comfortTopY < 0.0f ? 0.0f : comfortTopY;
        m_comfortZoneBottomY = comfortBottomY < 0.0f ? 0.0f : comfortBottomY;
}

float CameraRig::GetComfortZoneTopY() const
{
        return m_comfortZoneTopY;
}

float CameraRig::GetComfortZoneBottomY() const
{
        return m_comfortZoneBottomY;
}

std::optional<float> CameraRig::GetClampMinY() const
{
        return m_minYClamp;
}

std::optional<float> CameraRig::GetClampMaxY() const
{
        return m_maxYClamp;
}

float CameraRig::GetScrollY() const
{
        return m_scrollY;
}

void CameraRig::ResetVerticalScroll(const float3& targetPosition)
{
        if (!std::isfinite(targetPosition.y))
        {
                return;
        }

        m_scrollY = ClampScrollY(targetPosition.y);
        m_hasScrollY = true;
}

void CameraRig::ApplyExternalWrapDeltaY(float wrapDeltaY, float targetRawY)
{
        if (!std::isfinite(wrapDeltaY) || !std::isfinite(targetRawY))
        {
                return;
        }

        if (m_hasScrollY && std::isfinite(m_scrollY))
        {
                m_scrollY += wrapDeltaY;
        }
        else
        {
                m_scrollY = ClampScrollY(targetRawY);
                m_hasScrollY = true;
        }

        m_lastTargetYRaw = targetRawY;
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

void CameraRig::SetClampY(std::optional<float> minY, std::optional<float> maxY)
{
        m_minYClamp = minY;
        m_maxYClamp = maxY;
}

void CameraRig::SetWrapY(std::optional<float> minY, std::optional<float> maxY)
{
        m_minYWrap = minY;
        m_maxYWrap = maxY;
}

float CameraRig::ClampScrollY(float scrollY) const
{
        float clamped = scrollY;
        if (m_minYWrap && m_maxYWrap && std::isfinite(*m_minYWrap) && std::isfinite(*m_maxYWrap))
        {
                const float minY = std::min(*m_minYWrap, *m_maxYWrap);
                const float maxY = std::max(*m_minYWrap, *m_maxYWrap);
                const float range = maxY - minY;
                if (range > 0.0f)
                {
                        float offset = std::fmod(clamped - minY, range);
                        if (offset < 0.0f)
                        {
                                offset += range;
                        }
                        return minY + offset;
                }
        }
        if (m_minYClamp && std::isfinite(*m_minYClamp) && clamped < *m_minYClamp)
        {
                clamped = *m_minYClamp;
        }
        if (m_maxYClamp && std::isfinite(*m_maxYClamp) && clamped > *m_maxYClamp)
        {
                clamped = *m_maxYClamp;
        }
        return clamped;
}

void CameraRig::Attach(const std::shared_ptr<Entities::Entity>& target)
{
        m_target = target;
        m_targetRaw = target.get();
}

void CameraRig::Attach(Entities::Entity* target)
{
        m_target.reset();
        m_targetRaw = target;
}

void CameraRig::OnUpdate(const Core::UpdateContext& ctx)
{
        if (!m_camera)
        {
                return;
        }

        auto target = m_target.lock();
        Entities::Entity* targetEntity = target ? target.get() : m_targetRaw;
        if (!targetEntity)
        {
                return;
        }

        const auto& targetTransform = targetEntity->GetTransform();
        const float3 targetRotation = m_inheritRotation ? targetTransform.rotation : float3{ 0.0f, 0.0f, 0.0f };
        float3 targetPosition = targetTransform.position;
        const float rawTargetY = targetPosition.y;

        if (m_minYWrap && m_maxYWrap && std::isfinite(*m_minYWrap) && std::isfinite(*m_maxYWrap)
            && std::isfinite(rawTargetY))
        {
                const float minY = std::min(*m_minYWrap, *m_maxYWrap);
                const float maxY = std::max(*m_minYWrap, *m_maxYWrap);
                const float wrapRange = maxY - minY;
                if (wrapRange > 0.0f && m_lastTargetYRaw && std::isfinite(*m_lastTargetYRaw))
                {
                        const float delta = rawTargetY - *m_lastTargetYRaw;
                        if (std::fabs(delta) > wrapRange * 0.5f)
                        {
                                const float wrapDelta = delta > 0.0f ? wrapRange : -wrapRange;
                                if (m_hasScrollY)
                                {
                                        m_scrollY += wrapDelta;
                                }
                                if (m_camera)
                                {
                                        float3 cameraPosition = m_camera->GetPosition();
                                        if (std::isfinite(cameraPosition.y))
                                        {
                                                cameraPosition.y += wrapDelta;
                                                m_camera->SetPosition(cameraPosition);
                                        }
                                }
                        }
                }
        }
        m_lastTargetYRaw = rawTargetY;

        if (m_followPositionY)
        {
                if (std::isfinite(targetPosition.y))
                {
                        if (!m_hasScrollY)
                        {
                                m_scrollY = targetPosition.y;
                                m_hasScrollY = true;
                        }

                        if (m_comfortZoneTopY > 0.0f || m_comfortZoneBottomY > 0.0f)
                        {
                                const float lowerBound = m_scrollY - m_comfortZoneBottomY;
                                const float upperBound = m_scrollY + m_comfortZoneTopY;
                                if (targetPosition.y < lowerBound)
                                {
                                        m_scrollY = targetPosition.y + m_comfortZoneBottomY;
                                }
                                else if (targetPosition.y > upperBound)
                                {
                                        m_scrollY = targetPosition.y - m_comfortZoneTopY;
                                }
                        }
                        else
                        {
                                m_scrollY = targetPosition.y;
                        }

                        m_scrollY = ClampScrollY(m_scrollY);
                        targetPosition.y = m_scrollY;
                }
        }

        DirectX::XMVECTOR targetPos = CameraSpace::LoadFloat3(targetPosition);
        DirectX::XMVECTOR offsetVec = CameraSpace::RotateOffset(m_offset, targetRotation.y + m_yaw, targetRotation.x + m_pitch);
        DirectX::XMVECTOR desiredPos = DirectX::XMVectorAdd(targetPos, offsetVec);

        if (!m_lockTranslation)
        {
                DirectX::XMVECTOR currentPos = CameraSpace::LoadFloat3(m_camera->GetPosition());
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

                const float alphaXZ = Clamp01(1.0f - std::exp(-m_followSpeed * ctx.dt));
                const float alphaY  = Clamp01(1.0f - std::exp(-m_followSpeedY * ctx.dt));

                float newX = currentPosValue.x;
                float newY = currentPosValue.y;
                float newZ = currentPosValue.z;

                if (m_followPositionX)
                {
                        newX = currentPosValue.x +
                               (desiredPosValue.x - currentPosValue.x) * alphaXZ;
                }

                if (m_followPositionY)
                {
                        newY = currentPosValue.y +
                               (desiredPosValue.y - currentPosValue.y) * alphaY;
                }

                if (m_followPositionZ)
                {
                        newZ = currentPosValue.z +
                               (desiredPosValue.z - currentPosValue.z) * alphaXZ;
                }

                if (m_minYWrap && m_maxYWrap && std::isfinite(*m_minYWrap) && std::isfinite(*m_maxYWrap))
                {
                        const float minY = std::min(*m_minYWrap, *m_maxYWrap);
                        const float maxY = std::max(*m_minYWrap, *m_maxYWrap);
                        const float wrapRange = maxY - minY;
                        if (wrapRange > 0.0f && std::isfinite(newY))
                        {
                                float offset = std::fmod(newY - minY, wrapRange);
                                if (offset < 0.0f)
                                {
                                        offset += wrapRange;
                                }
                                newY = minY + offset;
                        }
                }

                m_camera->SetPosition(float3{ newX, newY, newZ });

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

}
