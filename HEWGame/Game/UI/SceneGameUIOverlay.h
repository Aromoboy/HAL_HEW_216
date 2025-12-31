#pragma once

#include "UIManager.h"

class SceneGameUIOverlay
{
public:
    SceneGameUIOverlay();
    ~SceneGameUIOverlay();

    bool Initialize(UIManager* uiManager);
    void Shutdown();

    void Update(const UIManager::HUDUpdateData& data);
    void Draw();

private:
    UIManager* m_uiManager = nullptr;
};
