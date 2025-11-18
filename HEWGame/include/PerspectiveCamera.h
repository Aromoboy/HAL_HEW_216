// ============================================================================
// 概要: PerspectiveCamera に関するインターフェースやデータ構造を宣言し、include から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#pragma once

#include "BaseCamera.h"
#include "Defines.h"

#include <string>

namespace CameraSpace
{
        class PerspectiveCamera : public BaseCamera
        {
        public:
                PerspectiveCamera(
                        float verticalFovDegrees = 60.0f,
                        float aspectRatio = static_cast<float>(SCREEN_WIDTH) / static_cast<float>(SCREEN_HEIGHT),
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
                void LogInvalidParameter(const char* label, float value, const std::string& detail) const;

        private:
                float m_fovY;
                float m_aspect;
                float m_nearZ;
                float m_farZ;
        };
}
