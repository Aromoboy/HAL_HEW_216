#include "BaseCamera.h"

#include "Defines.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

namespace
{
constexpr float kMaxCoordinateMagnitude = 100000.0f;
constexpr float kMaxRotationMagnitude = DirectX::XM_PI * 16.0f;
constexpr int kMaxPriorityMagnitude = 1000;
constexpr float kDefaultFovDegrees = 60.0f;
constexpr float kMinFovDegrees = 1.0f;
constexpr float kMaxFovDegrees = 175.0f;
constexpr float kDefaultAspect = static_cast<float>(SCREEN_WIDTH) / static_cast<float>(SCREEN_HEIGHT);
constexpr float kDefaultNearPlane = 0.1f;
constexpr float kDefaultFarPlane = 1000.0f;

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

struct PerspectiveParameters
{
float fovYDegrees;
float aspect;
float nearPlane;
float farPlane;
};

void DebugTrace(const std::string& message)
{
#if defined(_WIN32) && defined(_DEBUG)
::OutputDebugStringA(message.c_str());
#else
(void)message;
#endif
}

PerspectiveParameters SanitizePerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane, const char* context)
{
PerspectiveParameters result{ fovYDegrees, aspect, nearPlane, farPlane };
bool adjusted = false;
std::ostringstream warnings;
warnings << "[BaseCamera] " << context << " perspective adjusted: ";

if (!std::isfinite(result.fovYDegrees))
{
result.fovYDegrees = kDefaultFovDegrees;
adjusted = true;
warnings << "FOV=nan->" << result.fovYDegrees << "deg ";
}
result.fovYDegrees = std::clamp(result.fovYDegrees, kMinFovDegrees, kMaxFovDegrees);
if (result.fovYDegrees <= kMinFovDegrees || result.fovYDegrees >= kMaxFovDegrees)
{
adjusted = true;
warnings << "FOV clamped to " << result.fovYDegrees << "deg ";
}

if (!std::isfinite(result.aspect) || result.aspect <= 0.0f)
{
result.aspect = kDefaultAspect;
adjusted = true;
warnings << "aspect reset to " << result.aspect << ' ';
}

if (!std::isfinite(result.nearPlane) || result.nearPlane <= 0.0f)
{
result.nearPlane = kDefaultNearPlane;
adjusted = true;
warnings << "nearZ reset to " << result.nearPlane << ' ';
}

if (!std::isfinite(result.farPlane))
{
result.farPlane = kDefaultFarPlane;
adjusted = true;
warnings << "farZ reset to " << result.farPlane << ' ';
}

if (result.farPlane <= result.nearPlane)
{
result.farPlane = std::max(result.nearPlane + 1.0f, kDefaultFarPlane);
adjusted = true;
warnings << "farZ increased to " << result.farPlane << ' ';
}

if (adjusted)
{
warnings << '\n';
DebugTrace(warnings.str());
}

return result;
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
, m_fovYDegrees(kDefaultFovDegrees)
, m_aspect(kDefaultAspect)
, m_nearPlane(kDefaultNearPlane)
, m_farPlane(kDefaultFarPlane)
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
		const auto sanitized = SanitizePerspective(fovYDegrees, aspect, nearPlane, farPlane, "SetPerspective");
		
		m_fovYDegrees = sanitized.fovYDegrees;
		m_aspect = sanitized.aspect;
		m_nearPlane = sanitized.nearPlane;
		m_farPlane = sanitized.farPlane;
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
			DirectX::XMMATRIX scaleMatrix = DirectX::XMMatrixScaling(
				m_transform.scale.x,
				m_transform.scale.y,
				m_transform.scale.z);
			DirectX::XMMATRIX translationMatrix = DirectX::XMMatrixTranslation(eye.x, eye.y, eye.z);

			DirectX::XMVECTOR eyeVec = DirectX::XMVectorSet(eye.x, eye.y, eye.z, 1.0f);
			DirectX::XMVECTOR forward = DirectX::XMVector3TransformNormal(
				DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotationMatrix);
			DirectX::XMVECTOR upVector = DirectX::XMVector3TransformNormal(
				DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotationMatrix);

			const float forwardLengthSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(forward));
			if (!(forwardLengthSq > 1.0e-6f))
			{
				forward = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			}

			const float upLengthSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(upVector));
			if (!(upLengthSq > 1.0e-6f))
			{
				upVector = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			}

			DirectX::XMVECTOR focus = DirectX::XMVectorAdd(eyeVec, forward);
			DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eyeVec, focus, upVector);
			DirectX::XMStoreFloat4x4(&m_view, view);
			m_viewDirty = false;
			DirectX::XMFLOAT4X4 worldMatrix;
			DirectX::XMStoreFloat4x4(&worldMatrix, scaleMatrix * rotationMatrix * translationMatrix);

		#if defined(_DEBUG)
			assert(!MatrixHasNaN(worldMatrix));
			assert(!MatrixHasNaN(m_view));
		#endif

		#if defined(_DEBUG)
			DebugUpdateTransformSnapshot();
		#endif
		}

		void BaseCamera::UpdateProjection()
		{
			DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(
				DirectX::XMConvertToRadians(m_fovYDegrees),
				m_aspect,
				m_nearPlane,
				m_farPlane);
			DirectX::XMStoreFloat4x4(&m_projection, projection);
			m_projectionDirty = false;

		#if defined(_DEBUG)
			assert(!MatrixHasNaN(m_projection));
			assert(!MatrixHasNaN(m_view));
		#endif
		}

		void BaseCamera::MarkViewDirty()
		{
			m_viewDirty = true;
		}
		void BaseCamera::MarkViewDirty()
		{
			m_viewDirty = true;
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
