#include "TimerDisplay.h"
#include <DirectXMath.h>
#include "Defines.h"
#include "DirectX.h"

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
    Sprite::Init();
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
    if (!m_numberTexture) return;

    // Ԃ𕪂ƕbɕϊ
    int totalSeconds = static_cast<int>(remainingSeconds);
    if (totalSeconds < 0) totalSeconds = 0;

    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    int min10 = (minutes / 10) % 10;
    int min1 = minutes % 10;
    int sec10 = (seconds / 10) % 10;
    int sec1 = seconds % 10;

    DirectX::XMFLOAT4X4 identity;
    DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());

    SetDepthTest(false);
    SetBlendMode(BLEND_ALPHA);

    Sprite::SetWorld(identity);
    Sprite::SetView(identity);
    Sprite::SetProjection(identity);

    float startX = 0.60f;
    float startY = 0.85f;
    float spacing = 0.08f;

    DrawDigit(min10, startX, startY);
    DrawDigit(min1, startX + spacing, startY);

    DrawDigit(sec10, startX + spacing * 2.2f, startY);
    DrawDigit(sec1, startX + spacing * 3.2f, startY);

    SetDepthTest(true);
}

void TimerDisplay::DrawDigit(int digit, float x, float y)
{
    if (!m_numberTexture) return;

    float uvWidth = 0.1f;
    float uvX = digit * uvWidth;

    Sprite::SetTexture(m_numberTexture);
    Sprite::SetOffset(DirectX::XMFLOAT2(x, y));
    Sprite::SetSize(DirectX::XMFLOAT2(0.10f, 0.15f));
    Sprite::SetUVPos(DirectX::XMFLOAT2(uvX, 0.0f));
    Sprite::SetUVScale(DirectX::XMFLOAT2(uvWidth, 1.0f));
    Sprite::SetColor(DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));

    Sprite::Draw();
}
