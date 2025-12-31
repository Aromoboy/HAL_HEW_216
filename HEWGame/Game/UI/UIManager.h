#pragma once

#include <memory>
#include <optional>

#include "HudRenderer.h"
#include "ResultRenderer.h"
#include "TimerDisplay.h"

class Texture;

class UIManager
{
public:
    struct HUDUpdateData
    {
        HudDisplayData hud{};
        ResultDisplayData result{};
        bool showHud = false;
        bool showResult = false;
    };

    UIManager();
    ~UIManager();

    bool Initialize(const HudLayoutConfig& hudConfig, const ResultLayoutConfig& resultConfig);
    void Shutdown();

    void UpdateHUD(const HUDUpdateData& data);
    void Draw();

private:
    std::unique_ptr<HudRenderer> m_hudRenderer;
    std::unique_ptr<ResultRenderer> m_resultRenderer;
    TimerDisplay m_timerDisplay;
    std::unique_ptr<Texture> m_numberTexture;
    std::optional<HUDUpdateData> m_cachedData;
    bool m_timerReady = false;
};
