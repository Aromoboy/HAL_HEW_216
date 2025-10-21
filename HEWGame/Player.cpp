#include "Player.h"

#include <Windows.h>
#include <Xinput.h>
#include <algorithm>
#include <cmath>

#include "DirectX.h"
#include "Keyboard.h"

#pragma comment(lib, "xinput.lib")

namespace
{
        constexpr float kMinPitch = -DirectX::XM_PIDIV2 + 0.05f;
        constexpr float kMaxPitch = DirectX::XM_PIDIV2 - 0.05f;
}

Player::Player()
        : m_position{ 0.0f, 1.5f, -5.0f }
        , m_rotation{ 0.0f, 0.0f }
        , m_velocity{ 0.0f, 0.0f, 0.0f }
        , m_moveSpeed(6.0f)
        , m_mouseSensitivity(0.0025f)
        , m_gamepadSensitivity(1.0f)
        , m_fireCooldown(0.2f)
        , m_fireTimer(0.0f)
        , m_fireRequested(false)
        , m_mouseCaptured(false)
{
}

void Player::Initialize(const DirectX::XMFLOAT3& startPosition)
{
        m_position = startPosition;
        m_rotation = { 0.0f, 0.0f };
        m_velocity = { 0.0f, 0.0f, 0.0f };
        m_fireTimer = 0.0f;
        m_fireRequested = false;
        m_mouseCaptured = false;
}

void Player::Update(float deltaTime)
{
        if (deltaTime <= 0.0f)
        {
                deltaTime = 1.0f / 60.0f;
        }

        m_fireRequested = false;
        m_fireTimer = std::max(0.0f, m_fireTimer - deltaTime);

        UpdateMouse(deltaTime);

        m_velocity = { 0.0f, 0.0f, 0.0f };
        UpdateKeyboard(deltaTime);
        UpdateGamepad(deltaTime);

        m_position.x += m_velocity.x * deltaTime;
        m_position.y += m_velocity.y * deltaTime;
        m_position.z += m_velocity.z * deltaTime;
}

bool Player::ConsumeFireRequest()
{
        if (m_fireRequested)
        {
                m_fireRequested = false;
                return true;
        }
        return false;
}

DirectX::XMMATRIX Player::GetViewMatrix() const
{
        using namespace DirectX;
        XMFLOAT3 forward = GetForwardVector();
        XMVECTOR eye = XMLoadFloat3(&m_position);
        XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&forward));
        XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        return XMMatrixLookToLH(eye, dir, up);
}

DirectX::XMMATRIX Player::GetWorldMatrix() const
{
        using namespace DirectX;
        XMMATRIX rotation = XMMatrixRotationY(m_rotation.x);
        XMMATRIX scale = XMMatrixScaling(0.6f, 1.2f, 0.6f);
        XMMATRIX translation = XMMatrixTranslation(m_position.x, m_position.y - 0.9f, m_position.z);
        return scale * rotation * translation;
}

DirectX::XMFLOAT3 Player::GetPosition() const
{
        return m_position;
}

DirectX::XMFLOAT3 Player::GetForwardVector() const
{
        float cosPitch = std::cosf(m_rotation.y);
        DirectX::XMFLOAT3 forward{
                cosPitch * std::sinf(m_rotation.x),
                std::sinf(m_rotation.y),
                cosPitch * std::cosf(m_rotation.x)
        };
        return forward;
}

DirectX::XMFLOAT3 Player::GetMuzzlePosition() const
{
        DirectX::XMFLOAT3 forward = GetForwardVector();
        DirectX::XMFLOAT3 muzzle{
                m_position.x + forward.x * 0.8f,
                m_position.y + forward.y * 0.8f + 0.15f,
                m_position.z + forward.z * 0.8f
        };
        return muzzle;
}

void Player::UpdateKeyboard(float)
{
        using namespace DirectX;

        float yaw = m_rotation.x;
        XMVECTOR forward = XMVector3Normalize(XMVectorSet(std::sinf(yaw), 0.0f, std::cosf(yaw), 0.0f));
        XMVECTOR right = XMVector3Normalize(XMVectorSet(std::cosf(yaw), 0.0f, -std::sinf(yaw), 0.0f));
        XMVECTOR move = XMVectorZero();

        if (IsKeyPress('W'))
        {
                move = XMVectorAdd(move, forward);
        }
        if (IsKeyPress('S'))
        {
                move = XMVectorSubtract(move, forward);
        }
        if (IsKeyPress('D'))
        {
                move = XMVectorAdd(move, right);
        }
        if (IsKeyPress('A'))
        {
                move = XMVectorSubtract(move, right);
        }

        float speedMultiplier = IsKeyPress(VK_SHIFT) ? 1.4f : 1.0f;
        if (!XMVector3Equal(move, XMVectorZero()))
        {
                move = XMVector3Normalize(move) * (m_moveSpeed * speedMultiplier);
                XMVECTOR current = XMLoadFloat3(&m_velocity);
                current = XMVectorAdd(current, move);
                XMStoreFloat3(&m_velocity, current);
        }

        if (IsKeyTrigger(VK_SPACE) && m_fireTimer <= 0.0f)
        {
                m_fireRequested = true;
                m_fireTimer = m_fireCooldown;
        }

        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) && m_fireTimer <= 0.0f)
        {
                m_fireRequested = true;
                m_fireTimer = m_fireCooldown;
        }
}

void Player::UpdateMouse(float)
{
        HWND hwnd = GetGameWindowHandle();
        if (!hwnd)
        {
                return;
        }

        if (GetForegroundWindow() != hwnd)
        {
                m_mouseCaptured = false;
                return;
        }

        RECT rect{};
        if (!GetClientRect(hwnd, &rect))
        {
                return;
        }

        POINT center{ rect.left + (rect.right - rect.left) / 2, rect.top + (rect.bottom - rect.top) / 2 };
        ClientToScreen(hwnd, &center);

        if (!m_mouseCaptured)
        {
                SetCursorPos(center.x, center.y);
                m_mouseCaptured = true;
                return;
        }

        POINT cursor;
        if (!GetCursorPos(&cursor))
        {
                return;
        }

        LONG deltaX = cursor.x - center.x;
        LONG deltaY = cursor.y - center.y;

        if (deltaX == 0 && deltaY == 0)
        {
                return;
        }

        SetCursorPos(center.x, center.y);

        m_rotation.x += static_cast<float>(deltaX) * m_mouseSensitivity;
        m_rotation.y -= static_cast<float>(deltaY) * m_mouseSensitivity;
        m_rotation.y = std::clamp(m_rotation.y, kMinPitch, kMaxPitch);
}

void Player::UpdateGamepad(float deltaTime)
{
        XINPUT_STATE state;
        ZeroMemory(&state, sizeof(state));
        if (XInputGetState(0, &state) != ERROR_SUCCESS)
        {
                return;
        }

        using namespace DirectX;
        float yaw = m_rotation.x;
        XMVECTOR forward = XMVector3Normalize(XMVectorSet(std::sinf(yaw), 0.0f, std::cosf(yaw), 0.0f));
        XMVECTOR right = XMVector3Normalize(XMVectorSet(std::cosf(yaw), 0.0f, -std::sinf(yaw), 0.0f));

        auto normalizeThumb = [](SHORT value, SHORT deadZone)
        {
                if (std::abs(value) <= deadZone)
                {
                        return 0.0f;
                }
                float sign = value >= 0 ? 1.0f : -1.0f;
                float scaled = (std::abs(value) - deadZone) / (32767.0f - deadZone);
                return scaled * sign;
        };

        float leftX = normalizeThumb(state.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        float leftY = normalizeThumb(state.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);

        XMVECTOR move = XMVectorZero();
        move = XMVectorAdd(move, XMVectorScale(forward, leftY));
        move = XMVectorAdd(move, XMVectorScale(right, leftX));
        if (!XMVector3Equal(move, XMVectorZero()))
        {
                move = XMVector3Normalize(move) * (m_moveSpeed * m_gamepadSensitivity);
                XMVECTOR current = XMLoadFloat3(&m_velocity);
                current = XMVectorAdd(current, move);
                XMStoreFloat3(&m_velocity, current);
        }

        float rightX = normalizeThumb(state.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        float rightY = normalizeThumb(state.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);

        m_rotation.x += rightX * m_mouseSensitivity * m_gamepadSensitivity * 60.0f * deltaTime;
        m_rotation.y -= rightY * m_mouseSensitivity * m_gamepadSensitivity * 60.0f * deltaTime;
        m_rotation.y = std::clamp(m_rotation.y, kMinPitch, kMaxPitch);

        bool fire = (state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) ||
                (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
        if (fire && m_fireTimer <= 0.0f)
        {
                m_fireRequested = true;
                m_fireTimer = m_fireCooldown;
        }
}
