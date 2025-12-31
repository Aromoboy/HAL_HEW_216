#pragma once
#include "Sprite.h"
#include "Texture.h"

class TimerDisplay
{
public:
    TimerDisplay();
    ~TimerDisplay();

    void Init(Texture* numberTexture);          // 数字テクスチャを設定
    void Draw(float remainingSeconds);          // 残り時間を描画
    void SetPosition(float x, float y);         // 表示位置を設定
    void SetDigitSize(float width, float height); // 数字のサイズを設定

private:
    void DrawDigit(int digit, float x, float y); // 1桁の数字を描画

    Texture* m_numberTexture;   // 数字のテクスチャ
    float m_posX;               // 表示位置X
    float m_posY;               // 表示位置Y
    float m_digitWidth;         // 1桁の幅
    float m_digitHeight;        // 1桁の高さ
    float m_digitSpacing;       // 数字間の間隔
};
