// ============================================================================
// 概要: PerspectiveCamera の挙動や関連ロジックを実装し、src に属する機能を具体化している。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: src 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#include "PerspectiveCamera.h"

#include "DrawMatrix.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>

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
                std::ostringstream oss;
                oss << "[PerspectiveCamera] ctor fov(deg)=" << GetFieldOfViewDegrees()
                        << " aspect=" << m_aspect
                        << " nearZ=" << m_nearZ
                        << " farZ=" << m_farZ << "\n";
                DebugTrace(oss.str());

                MarkProjectionDirty();
        }

        void PerspectiveCamera::OnViewportChanged(float width, float height)
        {
                if (!IsFinitePositive(width) || !IsFinitePositive(height))
                {
                        std::ostringstream oss;
                        oss << "[PerspectiveCamera] OnViewportChanged rejected width=" << width
                                << " height=" << height << " (must be positive finite)\n";
                        DebugTrace(oss.str());
                        return;
                }

                const float newAspect = width / height;
                if (!std::isfinite(newAspect) || newAspect <= 0.0f)
                {
                        std::ostringstream oss;
                        oss << "[PerspectiveCamera] OnViewportChanged produced invalid aspect=" << newAspect
                                << " from width=" << width << " height=" << height << "\n";
                        DebugTrace(oss.str());
                        return;
                }

                if (std::fabs(newAspect - m_aspect) <= std::numeric_limits<float>::epsilon())
                {
                        return;
                }

                const float previous = m_aspect;
                m_aspect = newAspect;
                MarkProjectionDirty();
                UpdateProjection();

                std::ostringstream oss;
                oss << "[PerspectiveCamera] viewport updated aspect=" << m_aspect
                        << " (prev=" << previous << ") width=" << width << " height=" << height << "\n";
                DebugTrace(oss.str());
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
                        const float previousFar = m_farZ;
                        m_farZ = m_nearZ + kMinFarSeparation;

                        std::ostringstream oss;
                        oss << "[PerspectiveCamera] SetNearPlane adjusted farZ from " << previousFar
                                << " to maintain separation (now " << m_farZ << ")\n";
                        DebugTrace(oss.str());
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

#if defined(_DEBUG)
                std::ostringstream oss;
                oss << "[PerspectiveCamera] SetPerspective normalized fov(deg)=" << GetFieldOfViewDegrees()
                        << " aspect=" << m_aspect
                        << " nearZ=" << m_nearZ
                        << " farZ=" << m_farZ
                        << " -> dispatching to BaseCamera::SetPerspective" << "\n";
                DebugTrace(oss.str());
#endif

                BaseCamera::SetPerspective(DirectX::XMConvertToDegrees(m_fovY), m_aspect, m_nearZ, m_farZ);

                MarkProjectionDirty();
                UpdateProjection();
        }

        void PerspectiveCamera::UpdateProjection()
        {
                DrawMatrix::CreateProjectionMatrix(m_projection, m_fovY, m_aspect, m_nearZ, m_farZ);
                m_projectionDirty = false;
        }

        float PerspectiveCamera::NormalizeFovDegrees(float degrees, const char* contextLabel) const
        {
                if (!std::isfinite(degrees))
                {
                        std::ostringstream detail;
                        detail << contextLabel << " provided non-finite value; reverting to default " << kDefaultFovDegrees;
                        LogInvalidParameter("fovYDegrees", degrees, detail.str());
                        return DirectX::XMConvertToRadians(kDefaultFovDegrees);
                }

                const float clamped = std::clamp(degrees, kMinFovDegrees, kMaxFovDegrees);
                if (clamped != degrees)
                {
                        std::ostringstream detail;
                        detail << contextLabel << " clamped to range [" << kMinFovDegrees << ", " << kMaxFovDegrees << "]";
                        LogInvalidParameter("fovYDegrees", degrees, detail.str());
                }

                return DirectX::XMConvertToRadians(clamped);
        }

        float PerspectiveCamera::NormalizeAspect(float aspect, const char* contextLabel) const
        {
                if (!std::isfinite(aspect) || aspect <= kMinAspectRatio)
                {
                        const float fallback = DefaultAspectRatio();
                        std::ostringstream detail;
                        detail << contextLabel << " provided invalid aspect; using fallback " << fallback;
                        LogInvalidParameter("aspect", aspect, detail.str());
                        return fallback;
                }

                return aspect;
        }

        float PerspectiveCamera::NormalizeNearPlane(float nearPlane, const char* contextLabel) const
        {
                if (!std::isfinite(nearPlane) || nearPlane <= 0.0f)
                {
                        std::ostringstream detail;
                        detail << contextLabel << " provided invalid near plane; using default " << kDefaultNearPlane;
                        LogInvalidParameter("nearPlane", nearPlane, detail.str());
                        return kDefaultNearPlane;
                }

                return std::max(nearPlane, kDefaultNearPlane);
        }

        float PerspectiveCamera::NormalizeFarPlane(float farPlane, float nearPlane, const char* contextLabel) const
        {
                if (!std::isfinite(farPlane))
                {
                        std::ostringstream detail;
                        detail << contextLabel << " provided non-finite far plane; using default " << kDefaultFarPlane;
                        LogInvalidParameter("farPlane", farPlane, detail.str());
                        return std::max(kDefaultFarPlane, nearPlane + kMinFarSeparation);
                }

                if (farPlane <= nearPlane + kMinFarSeparation)
                {
                        const float adjusted = nearPlane + kMinFarSeparation;
                        std::ostringstream detail;
                        detail << contextLabel << " far plane <= near plane; adjusted to " << adjusted;
                        LogInvalidParameter("farPlane", farPlane, detail.str());
                        return adjusted;
                }

                return farPlane;
        }

        void PerspectiveCamera::LogInvalidParameter(const char* label, float value, const std::string& detail) const
        {
#if defined(_DEBUG)
                std::ostringstream oss;
                oss << "[PerspectiveCamera] invalid " << label << " value=" << value
                        << " -> " << detail << "\n";
                DebugTrace(oss.str());
#else
                (void)label;
                (void)value;
                (void)detail;
#endif
        }
}
