#include "SceneGameUIOverlay.h"

#include <string>

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

    constexpr const char* kLogPrefix = "[SceneGame] ";

    const HudLayoutConfig kHudLayoutConfig
    {
        512u,
        192u,
        32.0f,
        12.0f,
        28.0f,
        5.0f,
        1.5f,
        2.0f,
        DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
        DirectX::XMFLOAT4(1.0f, 0.55f, 0.25f, 1.0f)
    };

    const ResultLayoutConfig kResultLayoutConfig
    {
        512u,
        192u,
        32.0f,
        12.0f,
        28.0f,
        5.0f,
        1.5f,
        2.0f,
        DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
        DirectX::XMFLOAT4(1.0f, 0.55f, 0.25f, 1.0f)
    };
}

SceneGameUIOverlay::SceneGameUIOverlay() = default;

SceneGameUIOverlay::~SceneGameUIOverlay()
{
    Shutdown();
}

bool SceneGameUIOverlay::Initialize()
{
    m_uiManager = std::make_unique<UIManager>();
    if (m_uiManager && !m_uiManager->Initialize(kHudLayoutConfig, kResultLayoutConfig))
    {
        DebugTrace(std::string(kLogPrefix) + "UI manager failed to initialize; HUD/Result overlay disabled.\n");
        m_uiManager.reset();
    }

    return m_uiManager != nullptr;
}

void SceneGameUIOverlay::Shutdown()
{
    if (m_uiManager)
    {
        m_uiManager->Shutdown();
        m_uiManager.reset();
    }
}

void SceneGameUIOverlay::Update(const UIManager::HUDUpdateData& data)
{
    if (m_uiManager)
    {
        m_uiManager->UpdateHUD(data);
    }
}

void SceneGameUIOverlay::Draw()
{
    if (m_uiManager)
    {
        m_uiManager->Draw();
    }
}
