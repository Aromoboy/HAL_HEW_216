#include "TitleScene.h"

#include "Core/Input.h"
#include "Core/StepTimer.h"
#include "PerspectiveCamera.h"
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

    const bool createdNewCamera = !m_camera;
    if (!m_camera)
    {
        m_camera = std::make_shared<CameraSpace::PerspectiveCamera>();
    }

    auto registeredCamera = AddCamera(m_camera);
    if (!registeredCamera)
    {
#if defined(_DEBUG)
        DebugTrace(std::string(kLogPrefix) + "Failed to register title camera; RootDraw may lack an active camera.\n");
#endif
        return;
    }

    SetActiveCamera(registeredCamera);

#if defined(_DEBUG)
    std::ostringstream oss;
    oss << kLogPrefix << "Title camera " << (createdNewCamera ? "created" : "reused")
        << " ptr=" << registeredCamera.get();
    if (auto perspectiveCamera = std::dynamic_pointer_cast<CameraSpace::PerspectiveCamera>(registeredCamera))
    {
        oss << " fov=" << perspectiveCamera->GetFieldOfViewDegrees()
            << " aspect=" << perspectiveCamera->GetAspectRatio()
            << " near=" << perspectiveCamera->GetNearPlane()
            << " far=" << perspectiveCamera->GetFarPlane();
    }
    oss << " set active.\n";
    DebugTrace(oss.str());
#endif
}

void TitleScene::OnExit()
{
    LogLifecycle("Exit");

#if defined(_DEBUG)
    if (const auto* node = GetSceneUpdateNode())
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Unregistering title update node ptr=" << node
            << " phase=" << static_cast<int>(node->GetPhase())
            << " priority=" << node->GetPriority() << '\n';
        DebugTrace(oss.str());
    }
#endif

    UnregisterSceneUpdateNode();
}

void TitleScene::ConfigureUpdates()
{
    if (auto* input = GetInput())
    {
        Register(*input);
    }

    RegisterSceneUpdateNode(Core::UpdatePhase::Gameplay, 0);

#if defined(_DEBUG)
    if (const auto* node = GetSceneUpdateNode())
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Registered title update node ptr=" << node
            << " phase=" << static_cast<int>(node->GetPhase())
            << " priority=" << node->GetPriority() << '\n';
        DebugTrace(oss.str());
    }
#endif
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
