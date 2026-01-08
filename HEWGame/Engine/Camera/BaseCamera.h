#pragma once

#include "ICamera.h"
#include "Transform.h"
#include "Vector.h"

#include <DirectXMath.h>

class CameraRig;

namespace CameraSpace
{
        class BaseCamera : public ICamera
        {
        public:
                BaseCamera();
                explicit BaseCamera(const float3& position, const float3& rotation = { 0.0f, 0.0f, 0.0f });
                virtual ~BaseCamera() = default;

                const DirectX::XMFLOAT4X4& GetView() const override;
                const DirectX::XMFLOAT4X4& GetProjection() const override;

                void SetPosition(const float3& position);
                void SetRotation(const float3& rotation);
                void SetTransform(const Components::Transform& transform);
                void SetPriority(int priority);
                virtual void SetPerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane);

                const float3& GetPosition() const;
                const float3& GetRotation() const;
                int GetPriority() const;
                const Components::Transform& GetTransform() const;

        protected:
                void RecalculateView() override;
                virtual void UpdateProjection();

                void MarkViewDirty();
                void MarkProjectionDirty();

#if defined(_DEBUG)
                void DebugCheckTransformIntegrity(const char* caller) const;
                void DebugUpdateTransformSnapshot();
#endif

        protected:
                mutable DirectX::XMFLOAT4X4 m_projection;
                mutable bool m_projectionDirty;

        private:
                bool ValidatePosition(const float3& position) const;
                bool ValidateRotation(const float3& rotation) const;
                bool ValidatePriority(int priority) const;
                bool ValidatePerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane) const;

        private:
                Components::Transform m_transform;
                int m_priority;
                float m_fovYDegrees;
                float m_aspect;
                float m_nearPlane;
                float m_farPlane;

                mutable DirectX::XMFLOAT4X4 m_view;
                mutable bool m_viewDirty;

#if defined(_DEBUG)
                mutable Components::Transform m_debugLastTransformSnapshot;
                mutable bool m_debugHasTransformSnapshot;
#endif
        };
}
