// ============================================================================
// 概要: CameraManager に関するインターフェースやデータ構造を宣言し、include から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#pragma once

#include "ICamera.h"

#include <cstddef>
#include <memory>
#include <vector>

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

                void Update();

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

