#include "TimerDisplay.h"
#include "Defines.h"

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
    // Ԃ𕪂ƕbɕϊ
    int totalSeconds = static_cast<int>(remainingSeconds);
    if (totalSeconds < 0) totalSeconds = 0;

    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    int min10 = (minutes / 10) % 10;
    int min1 = minutes % 10;
    int sec10 = (seconds / 10) % 10;
    int sec1 = seconds % 10;

    // Timer digits are currently unused to avoid Sprite dependencies.
    (void)min10;
    (void)min1;
    (void)sec10;
    (void)sec1;
}

void TimerDisplay::DrawDigit(int digit, float x, float y)
{
    // Rendering is temporarily disabled to remove Sprite dependencies.
    (void)digit;
    (void)x;
    (void)y;
}
