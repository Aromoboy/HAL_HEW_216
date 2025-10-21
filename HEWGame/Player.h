#pragma once

#include <DirectXMath.h>

class Player
{
public:
        Player();

        void Initialize(const DirectX::XMFLOAT3& startPosition);
        void Update(float deltaTime);
        bool ConsumeFireRequest();

        DirectX::XMMATRIX GetViewMatrix() const;
        DirectX::XMMATRIX GetWorldMatrix() const;
        DirectX::XMFLOAT3 GetPosition() const;
        DirectX::XMFLOAT3 GetForwardVector() const;
        DirectX::XMFLOAT3 GetMuzzlePosition() const;

private:
        void UpdateKeyboard(float deltaTime);
        void UpdateMouse(float deltaTime);
        void UpdateGamepad(float deltaTime);

        DirectX::XMFLOAT3 m_position;
        DirectX::XMFLOAT2 m_rotation; // x = yaw, y = pitch
        DirectX::XMFLOAT3 m_velocity;
        float m_moveSpeed;
        float m_mouseSensitivity;
        float m_gamepadSensitivity;
        float m_fireCooldown;
        float m_fireTimer;
        bool m_fireRequested;
        bool m_mouseCaptured;
};
