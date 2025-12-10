#include "PerspectiveCamera.h"

#include "MatrixUtil.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
        constexpr float kDefaultFovDegrees = 60.0f;
        constexpr float kMinFovDegrees = 1.0f;
        constexpr float kMaxFovDegrees = 170.0f;

        constexpr float kDefaultNearPlane = 0.1f;
        constexpr float kDefaultFarPlane = 1000.0f;
        constexpr float kMinAspectRatio = 0.01f;
        constexpr float kMinFarSeparation = 0.01f;

        float DefaultAspectRatio()
        {
                return static_cast<float>(SCREEN_WIDTH) / static_cast<float>(SCREEN_HEIGHT);
        }

        bool IsFinitePositive(float value)
        {
                return std::isfinite(value) && value > 0.0f;
        }
}

namespace CameraSpace
{
        PerspectiveCamera::PerspectiveCamera(float verticalFovDegrees, float aspectRatio, float nearPlane, float farPlane)
                : m_fovY(NormalizeFovDegrees(verticalFovDegrees, "ctor"))
                , m_aspect(NormalizeAspect(aspectRatio, "ctor"))
                , m_nearZ(NormalizeNearPlane(nearPlane, "ctor"))
                , m_farZ(NormalizeFarPlane(farPlane, m_nearZ, "ctor"))
        {
                MarkProjectionDirty();
        }

        void PerspectiveCamera::OnViewportChanged(float width, float height)
        {
                if (!IsFinitePositive(width) || !IsFinitePositive(height))
                {
                        return;
                }

                const float newAspect = width / height;
                if (!std::isfinite(newAspect) || newAspect <= 0.0f)
                {
                        return;
                }

                if (std::fabs(newAspect - m_aspect) <= std::numeric_limits<float>::epsilon())
                {
                        return;
                }

                m_aspect = newAspect;
                MarkProjectionDirty();
                UpdateProjection();
        }

        void PerspectiveCamera::SetFieldOfViewDegrees(float fovYDegrees)
        {
                const float normalized = NormalizeFovDegrees(fovYDegrees, "SetFieldOfViewDegrees");
                if (std::fabs(normalized - m_fovY) <= std::numeric_limits<float>::epsilon())
                {
                        return;
                }

                m_fovY = normalized;
                MarkProjectionDirty();
        }

        float PerspectiveCamera::GetFieldOfViewDegrees() const
        {
                return DirectX::XMConvertToDegrees(m_fovY);
        }

        void PerspectiveCamera::SetNearPlane(float nearPlane)
        {
                const float normalized = NormalizeNearPlane(nearPlane, "SetNearPlane");
                if (std::fabs(normalized - m_nearZ) <= std::numeric_limits<float>::epsilon())
                {
                        return;
                }

                m_nearZ = normalized;
                if (m_farZ <= m_nearZ + kMinFarSeparation)
                {
                        m_farZ = m_nearZ + kMinFarSeparation;
                }

                MarkProjectionDirty();
        }

        float PerspectiveCamera::GetNearPlane() const
        {
                return m_nearZ;
        }

        void PerspectiveCamera::SetFarPlane(float farPlane)
        {
                const float normalized = NormalizeFarPlane(farPlane, m_nearZ, "SetFarPlane");
                if (std::fabs(normalized - m_farZ) <= std::numeric_limits<float>::epsilon())
                {
                        return;
                }

                m_farZ = normalized;
                MarkProjectionDirty();
        }

        float PerspectiveCamera::GetFarPlane() const
        {
                return m_farZ;
        }

        void PerspectiveCamera::SetClipPlanes(float nearPlane, float farPlane)
        {
                const float normalizedNear = NormalizeNearPlane(nearPlane, "SetClipPlanes.near");
                const float normalizedFar = NormalizeFarPlane(farPlane, normalizedNear, "SetClipPlanes.far");

                if (std::fabs(normalizedNear - m_nearZ) <= std::numeric_limits<float>::epsilon()
                        && std::fabs(normalizedFar - m_farZ) <= std::numeric_limits<float>::epsilon())
                {
                        return;
                }

                m_nearZ = normalizedNear;
                m_farZ = normalizedFar;
                MarkProjectionDirty();
        }

        float PerspectiveCamera::GetAspectRatio() const
        {
                return m_aspect;
        }

        void PerspectiveCamera::SetPerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane)
        {
                const float normalizedFov = NormalizeFovDegrees(fovYDegrees, "SetPerspective.fovY");
                const float normalizedAspect = NormalizeAspect(aspect, "SetPerspective.aspect");
                const float normalizedNear = NormalizeNearPlane(nearPlane, "SetPerspective.near");
                const float normalizedFar = NormalizeFarPlane(farPlane, normalizedNear, "SetPerspective.far");

                const float epsilon = std::numeric_limits<float>::epsilon();
                bool changed = false;

                if (std::fabs(normalizedFov - m_fovY) > epsilon)
                {
                        m_fovY = normalizedFov;
                        changed = true;
                }

                if (std::fabs(normalizedAspect - m_aspect) > epsilon)
                {
                        m_aspect = normalizedAspect;
                        changed = true;
                }

                if (std::fabs(normalizedNear - m_nearZ) > epsilon)
                {
                        m_nearZ = normalizedNear;
                        changed = true;
                }

                if (std::fabs(normalizedFar - m_farZ) > epsilon)
                {
                        m_farZ = normalizedFar;
                        changed = true;
                }

                if (!changed)
                {
                        return;
                }

                BaseCamera::SetPerspective(DirectX::XMConvertToDegrees(m_fovY), m_aspect, m_nearZ, m_farZ);

                MarkProjectionDirty();
                UpdateProjection();
        }

        void PerspectiveCamera::UpdateProjection()
        {
                m_projection = MathUtil::MakeProjectionMatrix(m_fovY, m_aspect, m_nearZ, m_farZ);
                m_projectionDirty = false;
        }

        float PerspectiveCamera::NormalizeFovDegrees(float degrees, const char* /*contextLabel*/) const
        {
                if (!std::isfinite(degrees))
                {
                        return DirectX::XMConvertToRadians(kDefaultFovDegrees);
                }

                const float clamped = std::clamp(degrees, kMinFovDegrees, kMaxFovDegrees);
                return DirectX::XMConvertToRadians(clamped);
        }

        float PerspectiveCamera::NormalizeAspect(float aspect, const char* /*contextLabel*/) const
        {
                if (!std::isfinite(aspect) || aspect <= kMinAspectRatio)
                {
                        const float fallback = DefaultAspectRatio();
                        return fallback;
                }

                return aspect;
        }

        float PerspectiveCamera::NormalizeNearPlane(float nearPlane, const char* /*contextLabel*/) const
        {
                if (!std::isfinite(nearPlane) || nearPlane <= 0.0f)
                {
                        return kDefaultNearPlane;
                }

                return std::max(nearPlane, kDefaultNearPlane);
        }

        float PerspectiveCamera::NormalizeFarPlane(float farPlane, float nearPlane, const char* /*contextLabel*/) const
        {
                if (!std::isfinite(farPlane))
                {
                        return std::max(kDefaultFarPlane, nearPlane + kMinFarSeparation);
                }

                if (farPlane <= nearPlane + kMinFarSeparation)
                {
                        const float adjusted = nearPlane + kMinFarSeparation;
                        return adjusted;
                }

                return farPlane;
        }
}
