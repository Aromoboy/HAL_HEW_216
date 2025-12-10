#include "TimerDisplay.h"
#include "DirectX.h"
#include "Defines.h"

#include <algorithm>

TimerDisplay::TimerDisplay()
    : m_numberTexture(nullptr)
    , m_posX(700.0f)
    , m_posY(50.0f)
    , m_digitWidth(50.0f)
    , m_digitHeight(70.0f)
    , m_digitSpacing(55.0f)
{
}

TimerDisplay::~TimerDisplay()
{
}

void TimerDisplay::Init(std::shared_ptr<Texture2D> numberTexture)
{
    m_numberTexture = std::move(numberTexture);
    m_quadRenderer = std::make_unique<UnifiedQuadRenderer>();
    m_ready = static_cast<bool>(m_numberTexture);
}

void TimerDisplay::SetPosition(float x, float y)
{
    m_posX = x;
    m_posY = y;
}

void TimerDisplay::SetDigitSize(float width, float height)
{
    m_digitWidth = width;
    m_digitHeight = height;
    m_digitSpacing = width + 5.0f;
}

void TimerDisplay::Draw(float remainingSeconds)
{
    if (!m_ready || !m_quadRenderer || !m_numberTexture)
    {
        return;
    }

    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    if (!m_quadRenderer->TryGetViewportSize(viewportWidth, viewportHeight))
    {
        return;
    }

    int totalSeconds = static_cast<int>(remainingSeconds);
    if (totalSeconds < 0) totalSeconds = 0;

    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    int min10 = (minutes / 10) % 10;
    int min1 = minutes % 10;
    int sec10 = (seconds / 10) % 10;
    int sec1 = seconds % 10;

    float drawX = m_posX;
    const float drawY = m_posY;

    DrawDigit(min10, drawX, drawY);
    drawX += m_digitSpacing;
    DrawDigit(min1, drawX, drawY);
    drawX += m_digitSpacing * 0.6f; // small gap for ':'
    DrawDigit(sec10, drawX, drawY);
    drawX += m_digitSpacing;
    DrawDigit(sec1, drawX, drawY);
}

void TimerDisplay::DrawDigit(int digit, float x, float y)
{
    if (digit < 0 || digit > 9 || !m_quadRenderer || !m_numberTexture)
    {
        return;
    }

    const float uvWidth = 0.1f;
    const float uMin = uvWidth * static_cast<float>(digit);

    UnifiedQuadRenderer::DrawParams params{};
    params.texture = m_numberTexture;
    params.tint = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    params.positionPx = DirectX::XMFLOAT2(x, y);
    params.sizePx = DirectX::XMFLOAT2(m_digitWidth, m_digitHeight);
    params.uvStart = DirectX::XMFLOAT2(uMin, 0.0f);
    params.uvSize = DirectX::XMFLOAT2(uvWidth, 1.0f);
    params.enableBlending = true;
    params.disableDepth = true;
    m_quadRenderer->Draw(params);
}

bool TimerDisplay::TryGetViewportSize(float& width, float& height) const
{
    return m_quadRenderer ? m_quadRenderer->TryGetViewportSize(width, height) : false;
}
