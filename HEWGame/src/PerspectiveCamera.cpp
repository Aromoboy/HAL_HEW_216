#include "PerspectiveCamera.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <sstream>

namespace
{
constexpr float kDefaultFovDegrees = 60.0f;
constexpr float kMinFovDegrees = 1.0f;
constexpr float kMaxFovDegrees = 175.0f;

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

void DebugTrace(const std::string& message)
{
#if defined(_WIN32) && defined(_DEBUG)
::OutputDebugStringA(message.c_str());
#else
(void)message;
#endif
}

bool MatrixHasNaN(const DirectX::XMFLOAT4X4& matrix)
{
const float* values = &matrix._11;
for (int i = 0; i < 16; ++i)
{
if (!std::isfinite(values[i]))
{
return true;
}
}
return false;
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
float rawAspect = DefaultAspectRatio();
if (IsFinitePositive(width) && IsFinitePositive(height))
{
rawAspect = width / height;
}

const float newAspect = NormalizeAspect(rawAspect, "OnViewportChanged.aspect");
if (std::fabs(newAspect - m_aspect) <= std::numeric_limits<float>::epsilon())
{
return;
}

m_aspect = newAspect;
MarkProjectionDirty();
UpdateProjection();
BaseCamera::SetPerspective(DirectX::XMConvertToDegrees(m_fovY), m_aspect, m_nearZ, m_farZ);
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
DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(m_fovY, m_aspect, m_nearZ, m_farZ);
DirectX::XMStoreFloat4x4(&m_projection, projection);
m_projectionDirty = false;

#if defined(_DEBUG)
assert(!MatrixHasNaN(m_projection));
#endif
}

float PerspectiveCamera::NormalizeFovDegrees(float degrees, const char* /*contextLabel*/) const
{
float result = degrees;
bool adjusted = false;
std::ostringstream oss;
oss << "[PerspectiveCamera] NormalizeFovDegrees: ";

if (!std::isfinite(result))
{
result = kDefaultFovDegrees;
adjusted = true;
oss << "nan->" << result << "deg ";
}

const float clamped = std::clamp(result, kMinFovDegrees, kMaxFovDegrees);
if (clamped != result)
{
adjusted = true;
oss << "clamped to " << clamped << "deg ";
}

if (adjusted)
{
oss << '\n';
DebugTrace(oss.str());
}

return DirectX::XMConvertToRadians(clamped);
}

float PerspectiveCamera::NormalizeAspect(float aspect, const char* /*contextLabel*/) const
{
float result = aspect;
bool adjusted = false;
std::ostringstream oss;
oss << "[PerspectiveCamera] NormalizeAspect: ";

if (!std::isfinite(result) || result <= kMinAspectRatio)
{
result = DefaultAspectRatio();
adjusted = true;
oss << "reset to " << result << ' ';
}

if (adjusted)
{
oss << '\n';
DebugTrace(oss.str());
}

return result;
}

float PerspectiveCamera::NormalizeNearPlane(float nearPlane, const char* /*contextLabel*/) const
{
float result = nearPlane;
bool adjusted = false;
std::ostringstream oss;
oss << "[PerspectiveCamera] NormalizeNearPlane: ";

if (!std::isfinite(result) || result <= 0.0f)
{
result = kDefaultNearPlane;
adjusted = true;
oss << "reset to " << result << ' ';
}

result = std::max(result, kDefaultNearPlane);
if (result != nearPlane)
{
adjusted = true;
oss << "clamped to " << result << ' ';
}

if (adjusted)
{
oss << '\n';
DebugTrace(oss.str());
}

return result;
}

float PerspectiveCamera::NormalizeFarPlane(float farPlane, float nearPlane, const char* /*contextLabel*/) const
{
float result = farPlane;
bool adjusted = false;
std::ostringstream oss;
oss << "[PerspectiveCamera] NormalizeFarPlane: ";

if (!std::isfinite(result))
{
result = std::max(kDefaultFarPlane, nearPlane + kMinFarSeparation);
adjusted = true;
oss << "reset to " << result << ' ';
}

if (result <= nearPlane + kMinFarSeparation)
{
result = nearPlane + kMinFarSeparation;
adjusted = true;
oss << "raised to " << result << ' ';
}

if (adjusted)
{
oss << '\n';
DebugTrace(oss.str());
}

return result;
}
}
