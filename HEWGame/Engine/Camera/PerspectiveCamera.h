#pragma once

#include "BaseCamera.h"

namespace CameraSpace
{
        class PerspectiveCamera : public BaseCamera
        {
        public:
                PerspectiveCamera(
                        float verticalFovDegrees = 60.0f,
                        float aspectRatio = 0.0f,
                        float nearPlane = 0.1f,
                        float farPlane = 1000.0f);

                void OnViewportChanged(float width, float height);

                void SetFieldOfViewDegrees(float fovYDegrees);
                float GetFieldOfViewDegrees() const;

                void SetNearPlane(float nearPlane);
                float GetNearPlane() const;

                void SetFarPlane(float farPlane);
                float GetFarPlane() const;

                void SetClipPlanes(float nearPlane, float farPlane);

                float GetAspectRatio() const;

                void SetPerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane) override;

        protected:
                void UpdateProjection() override;

        private:
                float NormalizeFovDegrees(float degrees, const char* contextLabel) const;
                float NormalizeAspect(float aspect, const char* contextLabel) const;
                float NormalizeNearPlane(float nearPlane, const char* contextLabel) const;
                float NormalizeFarPlane(float farPlane, float nearPlane, const char* contextLabel) const;

        private:
                float m_fovY;
                float m_aspect;
                float m_nearZ;
                float m_farZ;
        };
}
