#pragma once

#include <memory>

#include "UIManager.h"

class SceneGameUIOverlay
{
public:
    SceneGameUIOverlay();
    ~SceneGameUIOverlay();

    bool Initialize();
    void Shutdown();

    void Update(const UIManager::HUDUpdateData& data);
    void Draw();

private:
    std::unique_ptr<UIManager> m_uiManager;
};
