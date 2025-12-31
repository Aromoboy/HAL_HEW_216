#include "BaseCamera.h"

#include "Defines.h"
#include "DrawMatrix.h"

#include <cmath>

namespace
{
        constexpr float kMaxCoordinateMagnitude = 100000.0f;
        constexpr float kMaxRotationMagnitude = DirectX::XM_PI * 16.0f;
        constexpr int kMaxPriorityMagnitude = 1000;

        bool AreFloat3Equal(const float3& lhs, const float3& rhs)
        {
                return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
        }

        bool AreTransformsEqual(const Components::Transform& lhs, const Components::Transform& rhs)
        {
                return AreFloat3Equal(lhs.position, rhs.position)
                        && AreFloat3Equal(lhs.rotation, rhs.rotation)
                        && AreFloat3Equal(lhs.scale, rhs.scale);
        }

        bool ValidateFloat(const char* /*label*/, float value, float maxMagnitude)
        {
                if (std::isfinite(value) && std::fabs(value) <= maxMagnitude)
                {
                        return true;
                }
                return false;
        }

        bool ValidatePerspectiveValue(const char* /*label*/, float value)
        {
                if (std::isfinite(value) && value > 0.0f)
                {
                        return true;
                }
                return false;
        }
}

namespace CameraSpace
{
        BaseCamera::BaseCamera()
                : BaseCamera(float3{ 0.0f, 0.0f, 0.0f })
        {
        }

        BaseCamera::BaseCamera(const float3& position, const float3& rotation)
                : m_priority(0)
                , m_fovYDegrees(60.0f)
                , m_aspect(static_cast<float>(SCREEN_WIDTH) / static_cast<float>(SCREEN_HEIGHT))
                , m_nearPlane(CMETER(30.0f))
                , m_farPlane(METER(1000.0f))
                , m_viewDirty(true)
                , m_projectionDirty(true)
        {
                m_transform.position = position;
                m_transform.rotation = rotation;
                m_transform.scale = float3{ 1.0f, 1.0f, 1.0f };

                DirectX::XMStoreFloat4x4(&m_view, DirectX::XMMatrixIdentity());
                DirectX::XMStoreFloat4x4(&m_projection, DirectX::XMMatrixIdentity());

#if defined(_DEBUG)
                m_debugLastTransformSnapshot = m_transform;
                m_debugHasTransformSnapshot = true;
#endif
        }

        const DirectX::XMFLOAT4X4& BaseCamera::GetView() const
        {
#if defined(_DEBUG)
                DebugCheckTransformIntegrity("GetView");
#endif
                if (m_viewDirty)
                {
                        const_cast<BaseCamera*>(this)->RecalculateView();
                }
                return m_view;
        }

        const DirectX::XMFLOAT4X4& BaseCamera::GetProjection() const
        {
#if defined(_DEBUG)
                DebugCheckTransformIntegrity("GetProjection");
#endif
                if (m_projectionDirty)
                {
                        const_cast<BaseCamera*>(this)->UpdateProjection();
                }
                return m_projection;
        }

        void BaseCamera::SetPosition(const float3& position)
        {
                if (!ValidatePosition(position))
                {
                        return;
                }

                m_transform.position = position;
                MarkViewDirty();
                MarkProjectionDirty();
        }

        void BaseCamera::SetRotation(const float3& rotation)
        {
                if (!ValidateRotation(rotation))
                {
                        return;
                }

                m_transform.rotation = rotation;
                MarkViewDirty();
                MarkProjectionDirty();
        }

        void BaseCamera::SetTransform(const Components::Transform& transform)
        {
                if (!ValidatePosition(transform.position) || !ValidateRotation(transform.rotation))
                {
                        return;
                }

                const bool positionChanged = !AreFloat3Equal(transform.position, m_transform.position);
                const bool rotationChanged = !AreFloat3Equal(transform.rotation, m_transform.rotation);
                const bool scaleChanged = !AreFloat3Equal(transform.scale, m_transform.scale);

                if (!positionChanged && !rotationChanged && !scaleChanged)
                {
                        return;
                }

                m_transform = transform;

                if (positionChanged || rotationChanged)
                {
                        MarkViewDirty();
                        MarkProjectionDirty();
                }
        }

        void BaseCamera::SetPriority(int priority)
        {
                if (!ValidatePriority(priority))
                {
                        return;
                }

                m_priority = priority;
        }

        void BaseCamera::SetPerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane)
        {
                if (!ValidatePerspective(fovYDegrees, aspect, nearPlane, farPlane))
                {
                        return;
                }

                m_fovYDegrees = fovYDegrees;
                m_aspect = aspect;
                m_nearPlane = nearPlane;
                m_farPlane = farPlane;
                MarkProjectionDirty();
        }

        const float3& BaseCamera::GetPosition() const
        {
                return m_transform.position;
        }

        const float3& BaseCamera::GetRotation() const
        {
                return m_transform.rotation;
        }

        int BaseCamera::GetPriority() const
        {
                return m_priority;
        }

        const Components::Transform& BaseCamera::GetTransform() const
        {
                return m_transform;
        }

        void BaseCamera::RecalculateView()
        {
                const float3 eye = m_transform.position;

                DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(
                        m_transform.rotation.x,
                        m_transform.rotation.y,
                        m_transform.rotation.z);

                DirectX::XMVECTOR forward = DirectX::XMVector3TransformNormal(
                        DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotationMatrix);
                DirectX::XMVECTOR upVector = DirectX::XMVector3TransformNormal(
                        DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotationMatrix);

                float3 focus
                {
                        eye.x + DirectX::XMVectorGetX(forward),
                        eye.y + DirectX::XMVectorGetY(forward),
                        eye.z + DirectX::XMVectorGetZ(forward)
                };

                float3 up
                {
                        DirectX::XMVectorGetX(upVector),
                        DirectX::XMVectorGetY(upVector),
                        DirectX::XMVectorGetZ(upVector)
                };

                DrawMatrix::CreateViewMatrix(m_view, eye, focus, up);
                m_viewDirty = false;

#if defined(_DEBUG)
                DebugUpdateTransformSnapshot();
#endif
        }

        void BaseCamera::UpdateProjection()
        {
                DrawMatrix::CreateProjectionMatrix(
                        m_projection,
                        DirectX::XMConvertToRadians(m_fovYDegrees),
                        m_aspect,
                        m_nearPlane,
                        m_farPlane);
                m_projectionDirty = false;
        }

        void BaseCamera::MarkViewDirty()
        {
                m_viewDirty = true;
        }

        void BaseCamera::MarkProjectionDirty()
        {
                m_projectionDirty = true;
        }

        bool BaseCamera::ValidatePosition(const float3& position) const
        {
                return ValidateFloat("SetPosition.x", position.x, kMaxCoordinateMagnitude)
                        && ValidateFloat("SetPosition.y", position.y, kMaxCoordinateMagnitude)
                        && ValidateFloat("SetPosition.z", position.z, kMaxCoordinateMagnitude);
        }

        bool BaseCamera::ValidateRotation(const float3& rotation) const
        {
                return ValidateFloat("SetRotation.x", rotation.x, kMaxRotationMagnitude)
                        && ValidateFloat("SetRotation.y", rotation.y, kMaxRotationMagnitude)
                        && ValidateFloat("SetRotation.z", rotation.z, kMaxRotationMagnitude);
        }

        bool BaseCamera::ValidatePriority(int priority) const
        {
                if (std::abs(priority) <= kMaxPriorityMagnitude)
                {
                        return true;
                }
                return false;
        }

        bool BaseCamera::ValidatePerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane) const
        {
                bool fovValid = ValidatePerspectiveValue("fovYDegrees", fovYDegrees);
                bool aspectValid = ValidatePerspectiveValue("aspect", aspect);
                bool nearValid = ValidatePerspectiveValue("nearPlane", nearPlane);
                bool farValid = ValidatePerspectiveValue("farPlane", farPlane);

                if (!fovValid || !aspectValid || !nearValid || !farValid)
                {
                        return false;
                }

                if (farPlane <= nearPlane)
                {
                        return false;
                }

                return true;
        }

#if defined(_DEBUG)
        void BaseCamera::DebugCheckTransformIntegrity(const char* caller) const
        {
                if (!m_debugHasTransformSnapshot)
                {
                        return;
                }

                if (!m_viewDirty && !AreTransformsEqual(m_transform, m_debugLastTransformSnapshot))
                {
                        auto* self = const_cast<BaseCamera*>(this);
                        self->MarkViewDirty();
                        self->MarkProjectionDirty();
                }
        }

        void BaseCamera::DebugUpdateTransformSnapshot()
        {
                m_debugLastTransformSnapshot = m_transform;
                m_debugHasTransformSnapshot = true;
        }
#endif
}
