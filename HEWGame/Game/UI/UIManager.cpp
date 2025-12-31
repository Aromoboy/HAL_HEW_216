// 画面表示のまとめ役として各表示を更新する
// シーンゲームの更新と描画から呼ばれる

#include "UIManager.h"
#include "Texture.h"

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

    constexpr const char* kLogPrefix = "[SceneGame] ";
}

UIManager::UIManager() = default;

UIManager::~UIManager()
{
    Shutdown();
}

bool UIManager::Initialize(const HudLayoutConfig& hudConfig, const ResultLayoutConfig& resultConfig)
{
    m_hudRenderer = std::make_unique<HudRenderer>();
    if (m_hudRenderer && !m_hudRenderer->Initialize(hudConfig))
    {
        DebugTrace(std::string(kLogPrefix) + "HUD renderer failed to initialize; overlay disabled.\n");
        m_hudRenderer.reset();
    }
    else if (m_hudRenderer)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "HUD renderer initialized (texture="
            << hudConfig.textureWidth << 'x' << hudConfig.textureHeight
            << ", margin=" << hudConfig.marginPixels
            << ", warningThreshold=" << hudConfig.warningThresholdSeconds << "s).\n";
        ::DebugTrace(oss.str());
    }

    m_resultRenderer = std::make_unique<ResultRenderer>();
    if (m_resultRenderer && !m_resultRenderer->Initialize(resultConfig))
    {
        DebugTrace(std::string(kLogPrefix) + "HUD renderer failed to initialize; overlay disabled.\n");
        m_resultRenderer.reset();
    }
    else if (m_resultRenderer)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "Result renderer initialized (texture="
            << resultConfig.textureWidth << 'x' << resultConfig.textureHeight
            << ", margin=" << resultConfig.marginPixels
            << ", warningThreshold=" << resultConfig.warningThresholdSeconds << "s).\n";
        ::DebugTrace(oss.str());
    }

    m_numberTexture = std::make_unique<Texture>();
    if (FAILED(m_numberTexture->Create("Assets/Texture/number.png")))
    {
        DebugTrace(std::string(kLogPrefix) + "Timer number texture failed to initialize; timer display disabled.\n");
        m_numberTexture.reset();
        m_timerReady = false;
    }
    else
    {
        m_timerDisplay.Init(m_numberTexture.get());
        m_timerDisplay.SetPosition(700.0f, 50.0f);
        m_timerDisplay.SetDigitSize(50.0f, 70.0f);
        m_timerReady = true;
    }

    return m_hudRenderer || m_resultRenderer || m_timerReady;
}

void UIManager::Shutdown()
{
    if (m_resultRenderer)
    {
        m_resultRenderer->Shutdown();
        m_resultRenderer.reset();
    }

    if (m_hudRenderer)
    {
        m_hudRenderer->Shutdown();
        m_hudRenderer.reset();
    }

    m_numberTexture.reset();
    m_timerReady = false;
    m_cachedData.reset();
}

void UIManager::UpdateHUD(const HUDUpdateData& data)
{
    m_cachedData = data;
}

void UIManager::Draw()
{
    if (!m_cachedData)
    {
        return;
    }

    const HUDUpdateData& data = *m_cachedData;
    if (m_hudRenderer && data.showHud)
    {
        m_hudRenderer->Draw(data.hud);
    }

    if (m_resultRenderer && data.showResult)
    {
        m_resultRenderer->Draw(data.result);
    }

    if (m_timerReady)
    {
        const float remainingSeconds = data.showResult
            ? data.result.remainingSeconds
            : data.hud.remainingSeconds;
        m_timerDisplay.Draw(remainingSeconds);
    }
}
