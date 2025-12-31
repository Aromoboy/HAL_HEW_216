#include "CameraManager.h"

#include <algorithm>
#include <sstream>
#include <string>

#include "Core/IUpdatable.h"

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

        constexpr const char* kLogPrefix = "[CameraManager] ";
}

namespace CameraSpace
{
        CameraManager::CameraManager()
                : m_warnedAboutMissingCamera(false)
        {
        }

        std::shared_ptr<ICamera> CameraManager::AddCamera(std::shared_ptr<ICamera> camera)
        {
                if (!camera)
                {
                        return nullptr;
                }

                auto found = std::find_if(m_cameras.begin(), m_cameras.end(),
                        [&](const std::shared_ptr<ICamera>& existing)
                        {
                                return existing.get() == camera.get();
                        });

                if (found != m_cameras.end())
                {
                        ++m_stats.duplicateAdds;
                        return *found;
                }

                m_cameras.push_back(camera);
                ++m_stats.registeredCameras;
                return camera;
        }

        void CameraManager::SetActive(const std::shared_ptr<ICamera>& camera)
        {
                if (!camera)
                {
                        if (!m_active.expired())
                        {
                        }
                        m_active.reset();
                        return;
                }

                if (!Contains(camera))
                {
                        m_cameras.push_back(camera);
                        ++m_stats.registeredCameras;
                }

                m_active = camera;
                m_warnedAboutMissingCamera = false;
        }

        std::shared_ptr<ICamera> CameraManager::GetActiveCamera() const
        {
                return m_active.lock();
        }

        void CameraManager::Update(const Core::UpdateContext& ctx)
        {
                ++m_stats.updateCalls;
                (void)ctx;
                auto active = m_active.lock();
                if (!active)
                {
                        ++m_stats.framesWithoutActive;
#if defined(_DEBUG)
                        if (!m_warnedAboutMissingCamera)
                        {
                                std::ostringstream oss;
                                oss << kLogPrefix << "No active camera set. framesWithoutActive="
                                        << m_stats.framesWithoutActive << '\n';
                                DebugTrace(oss.str());
                                m_warnedAboutMissingCamera = true;
                        }
#endif
                        return;
                }

                (void)active->GetView();
                (void)active->GetProjection();
        }

        const CameraManager::ExecutionStats& CameraManager::GetExecutionStats() const
        {
                return m_stats;
        }

        const std::vector<std::shared_ptr<ICamera>>& CameraManager::GetCameras() const
        {
                return m_cameras;
        }

        bool CameraManager::Contains(const std::shared_ptr<ICamera>& camera) const
        {
                if (!camera)
                {
                        return false;
                }

                return std::any_of(m_cameras.begin(), m_cameras.end(),
                        [&](const std::shared_ptr<ICamera>& existing)
                        {
                                return existing.get() == camera.get();
                        });
        }
}

