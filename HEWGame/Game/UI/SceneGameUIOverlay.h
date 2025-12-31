// シーンゲームの画面表示を重ねて描く
// シーンゲームの描画から呼ばれる

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
