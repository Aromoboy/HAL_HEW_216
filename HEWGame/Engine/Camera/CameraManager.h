// エンジン内のカメラ切り替えをまとめている
// シーン描画が現在のカメラを参照する

#pragma once

#include "ICamera.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace Core
{
        struct UpdateContext;
}

namespace CameraSpace
{
        class CameraManager
        {
        public:
                struct ExecutionStats
                {
                        std::size_t registeredCameras = 0;
                        std::size_t duplicateAdds = 0;
                        std::size_t framesWithoutActive = 0;
                        std::size_t updateCalls = 0;
                };

                CameraManager();
                ~CameraManager() = default;

                std::shared_ptr<ICamera> AddCamera(std::shared_ptr<ICamera> camera);
                void SetActive(const std::shared_ptr<ICamera>& camera);
                std::shared_ptr<ICamera> GetActiveCamera() const;

                void Update(const Core::UpdateContext& ctx);

                const ExecutionStats& GetExecutionStats() const;
                const std::vector<std::shared_ptr<ICamera>>& GetCameras() const;

        private:
                bool Contains(const std::shared_ptr<ICamera>& camera) const;

        private:
                std::vector<std::shared_ptr<ICamera>> m_cameras;
                std::weak_ptr<ICamera> m_active;
                ExecutionStats m_stats;
                bool m_warnedAboutMissingCamera;
        };
}

