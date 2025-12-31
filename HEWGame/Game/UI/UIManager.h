// プレイ中に出す得点や結果表示の見た目をまとめたクラスだ
// 受け取った表示データに合わせて数字や文字の描画を組み立てる
// 出力は画面上の表示に使われる
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
