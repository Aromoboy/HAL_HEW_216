#pragma once
#include <memory>
#include "TextureDX.h"
#include "UnifiedQuadRenderer.h"

class TimerDisplay
{
public:
    TimerDisplay();
    ~TimerDisplay();

    void Init(std::shared_ptr<Texture2D> numberTexture);          // eNX`ݒ
    void Draw(float remainingSeconds);          // c莞Ԃ`
    void SetPosition(float x, float y);         // \ʒuݒ
    void SetDigitSize(float width, float height); // ̃TCYݒ

private:
    void DrawDigit(int digit, float x, float y); // 1̐`
    bool TryGetViewportSize(float& width, float& height) const;

    std::unique_ptr<UnifiedQuadRenderer> m_quadRenderer;

    std::shared_ptr<Texture2D> m_numberTexture;   // ̃eNX`
    float m_posX;               // \ʒuX
    float m_posY;               // \ʒuY
    float m_digitWidth;         // 1̕
    float m_digitHeight;        // 1̍
    float m_digitSpacing;       // Ԃ̊Ԋu
    bool m_ready = false;
};
