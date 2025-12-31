// 制限時間などの表示を描く役目を持つ
// 画面表示の更新と描画から呼ばれる

#pragma once
#include "Sprite.h"
#include "Texture.h"

class TimerDisplay
{
public:
    TimerDisplay();
    ~TimerDisplay();

    void Init(Texture* numberTexture);          // eNX`ݒ
    void Draw(float remainingSeconds);          // c莞Ԃ`
    void SetPosition(float x, float y);         // \ʒuݒ
    void SetDigitSize(float width, float height); // ̃TCYݒ

private:
    void DrawDigit(int digit, float x, float y); // 1̐`

    Texture* m_numberTexture;   // ̃eNX`
    float m_posX;               // \ʒuX
    float m_posY;               // \ʒuY
    float m_digitWidth;         // 1̕
    float m_digitHeight;        // 1̍
    float m_digitSpacing;       // Ԃ̊Ԋu
};
