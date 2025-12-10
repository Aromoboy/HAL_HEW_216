#pragma once
#include <array>
#include <memory>
#include "MeshDX.h"
#include "ModelRender.h"
#include "TextureDX.h"

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
    std::shared_ptr<MeshDX> GetDigitMesh(int digit);

    std::unique_ptr<ModelRender> m_modelRenderer;
    std::array<std::shared_ptr<MeshDX>, 10> m_digitMeshes{};

    std::shared_ptr<Texture2D> m_numberTexture;   // ̃eNX`
    float m_posX;               // \ʒuX
    float m_posY;               // \ʒuY
    float m_digitWidth;         // 1̕
    float m_digitHeight;        // 1̍
    float m_digitSpacing;       // Ԃ̊Ԋu
    bool m_ready = false;
};
