#include "BaseCamera.h"

#include "Defines.h"
#include "DrawMatrix.h"

#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>

#if defined(_WIN32)
#       ifndef NOMINMAX
#               define NOMINMAX
#       endif
#       include <windows.h>
#endif

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
        void DebugTrace(const std::string& message)
        {
                ::OutputDebugStringA(message.c_str());
        }
#else
        void DebugTrace(const std::string& message)
        {
                (void)message;
        }
#endif

        constexpr float kMaxCoordinateMagnitude = 100000.0f;
        constexpr float kMaxRotationMagnitude = DirectX::XM_PI * 16.0f;
        constexpr int kMaxPriorityMagnitude = 1000;

        bool ValidateFloat(const char* label, float value, float maxMagnitude)
        {
                if (std::isfinite(value) && std::fabs(value) <= maxMagnitude)
                {
                        return true;
                }

#if defined(_DEBUG)
                std::ostringstream oss;
                oss << "[Camera] " << label << " invalid value=" << value
                        << " (|value| must be <= " << maxMagnitude << ")\n";
                DebugTrace(oss.str());
#else
                (void)label;
                (void)value;
                (void)maxMagnitude;
#endif
                return false;
        }

        bool ValidatePerspectiveValue(const char* label, float value)
        {
                if (std::isfinite(value) && value > 0.0f)
                {
                        return true;
                }
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << "[Camera] invalid " << label << "=" << value << " (must be > 0)\n";
                DebugTrace(oss.str());
#else
                (void)label;
                (void)value;
#endif
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
        }

        const DirectX::XMFLOAT4X4& BaseCamera::GetView() const
        {
                if (m_viewDirty)
                {
                        const_cast<BaseCamera*>(this)->RecalculateView();
                }
                return m_view;
        }

        const DirectX::XMFLOAT4X4& BaseCamera::GetProjection() const
        {
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
        }

        void BaseCamera::SetRotation(const float3& rotation)
        {
                if (!ValidateRotation(rotation))
                {
                        return;
                }

                m_transform.rotation = rotation;
                MarkViewDirty();
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

        Components::Transform& BaseCamera::GetTransform()
        {
                return m_transform;
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
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << "[Camera] priority out of range value=" << priority
                        << " (|value| must be <= " << kMaxPriorityMagnitude << ")\n";
                DebugTrace(oss.str());
#else
                (void)priority;
#endif
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
#if defined(_DEBUG)
                        std::ostringstream oss;
                        oss << "[Camera] invalid frustum far=" << farPlane
                                << " must be greater than near=" << nearPlane << "\n";
                        DebugTrace(oss.str());
#endif
                        return false;
                }

                return true;
        }
}
