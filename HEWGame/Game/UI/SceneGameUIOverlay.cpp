#include "SceneGameUIOverlay.h"

SceneGameUIOverlay::SceneGameUIOverlay() = default;

SceneGameUIOverlay::~SceneGameUIOverlay()
{
    Shutdown();
}

bool SceneGameUIOverlay::Initialize(UIManager* uiManager)
{
    m_uiManager = uiManager;
    return m_uiManager != nullptr;
}

void SceneGameUIOverlay::Shutdown()
{
    m_uiManager = nullptr;
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
