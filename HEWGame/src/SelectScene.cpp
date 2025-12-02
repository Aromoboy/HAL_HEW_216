#include "SelectScene.h"

#include "Core/Input.h"
#include "Core/StepTimer.h"

#include <sstream>

#if defined(_WIN32)
#   ifndef NOMINMAX
#       define NOMINMAX
#   endif
#   include <windows.h>
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

    constexpr const char* kLogPrefix = "[SelectScene] ";
}

SelectScene::SelectScene() = default;

void SelectScene::Load(Core::Input& input)
{
    (void)input;
    LogLifecycle("Load");
}

void SelectScene::Update(Core::StepTimer& timer, Core::Input& input)
{
    RootUpdate(timer, input);
}

void SelectScene::Draw()
{
    RootDraw();
}

void SelectScene::OnEnter()
{
    LogLifecycle("Enter");
}

void SelectScene::OnExit()
{
    LogLifecycle("Exit");
}

void SelectScene::ConfigureUpdates()
{
    if (auto* input = GetInput())
    {
        Register(*input);
    }
}

void SelectScene::Update(Core::UpdateContext& ctx)
{
    (void)ctx;
}

void SelectScene::Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera)
{
    (void)activeCamera;
}

void SelectScene::LogLifecycle(const char* label) const
{
    std::ostringstream oss;
    oss << kLogPrefix << label << " scenePtr=" << this << '\n';
    DebugTrace(oss.str());
}
