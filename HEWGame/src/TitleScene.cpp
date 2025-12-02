#include "TitleScene.h"

#include "Core/Input.h"
#include "Core/StepTimer.h"
#include "SceneGame.h"

#include <memory>
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

    constexpr const char* kLogPrefix = "[TitleScene] ";
}

TitleScene::TitleScene(SceneSpace::SceneManager& manager)
    : m_manager(&manager)
{
    LogLifecycle("Construct");
}

void TitleScene::Load(Core::Input& input)
{
    (void)input;
    LogLifecycle("Load");
}

void TitleScene::Update(Core::StepTimer& timer, Core::Input& input)
{
    RootUpdate(timer, input);
}

void TitleScene::Draw()
{
    RootDraw();
}

void TitleScene::OnEnter()
{
    LogLifecycle("Enter");
}

void TitleScene::OnExit()
{
    LogLifecycle("Exit");
}

void TitleScene::ConfigureUpdates()
{
    if (auto* input = GetInput())
    {
        Register(*input);
    }
}

void TitleScene::Update(Core::UpdateContext& ctx)
{
    if (ctx.input.IsTrigger(Core::ActionId::Confirm))
    {
        LogLifecycle("StartRequested");
        if (m_manager)
        {
            m_manager->RequestSceneChange(std::make_unique<SceneGame>());
        }
    }
}

void TitleScene::Draw(const std::shared_ptr<CameraSpace::ICamera>& activeCamera)
{
    (void)activeCamera;
}

void TitleScene::LogLifecycle(const char* label) const
{
    std::ostringstream oss;
    oss << kLogPrefix << label << " scenePtr=" << this;
    if (m_manager)
    {
        oss << " managerPtr=" << m_manager;
    }
    oss << '\n';
    DebugTrace(oss.str());
}
