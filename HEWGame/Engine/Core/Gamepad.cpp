#include "Core/Gamepad.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr float kStickNorm = 32767.0f;
}

namespace Core
{
Gamepad::Gamepad()
: m_state{}
, m_prevState{}
, m_connected(false)
{
}

void Gamepad::Poll(int index)
{
m_prevState = m_state;
const bool wasConnected = m_connected;

XINPUT_STATE state{};
DWORD result = XInputGetState(index, &state);

if (result == ERROR_SUCCESS)
{
m_connected = true;
m_state = state;
}
else
{
m_connected = false;
ZeroMemory(&m_state, sizeof(XINPUT_STATE));
}
}

bool Gamepad::IsConnected() const
{
return m_connected;
}

bool Gamepad::IsButtonDown(WORD button) const
{
return m_connected && (m_state.Gamepad.wButtons & button) != 0;
}

bool Gamepad::WasButtonDown(WORD button) const
{
return m_connected && (m_prevState.Gamepad.wButtons & button) != 0;
}

float Gamepad::AxisLX() const { return m_connected ? NormalizeStickAxis(m_state.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) : 0.0f; }
float Gamepad::AxisLY() const { return m_connected ? NormalizeStickAxis(m_state.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) : 0.0f; }
float Gamepad::AxisRX() const { return m_connected ? NormalizeStickAxis(m_state.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) : 0.0f; }
float Gamepad::AxisRY() const { return m_connected ? NormalizeStickAxis(m_state.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) : 0.0f; }

float Gamepad::NormalizeStickAxis(short value, int deadZone)
{
const float magnitude = std::clamp(std::fabs(static_cast<float>(value)), 0.0f, kStickNorm);
const float threshold = static_cast<float>(std::clamp(deadZone, 0, 32766));
if (magnitude <= threshold)
{
return 0.0f;
}

// ミカ：デッドゾーンの外側を再正規化し負の最大値も範囲内に収める
const float normalized = (magnitude - threshold) / (kStickNorm - threshold);
return value < 0 ? -normalized : normalized;
}
}

