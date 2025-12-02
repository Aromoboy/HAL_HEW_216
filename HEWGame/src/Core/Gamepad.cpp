#include "Core/Gamepad.h"

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

if (m_connected != wasConnected)
{
OutputDebugStringA(m_connected
? "[Core::Gamepad] コントローラを検出しました（状態を更新）。推奨: そのまま入力テストを継続してください。\n"
: "[Core::Gamepad] コントローラが切断されました（状態を初期化）。推奨: ケーブル/電源を確認し、再接続後に挙動を確認してください。\n");
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

float Gamepad::AxisLX() const { return m_connected ? Normalize(m_state.Gamepad.sThumbLX) : 0.0f; }
float Gamepad::AxisLY() const { return m_connected ? Normalize(m_state.Gamepad.sThumbLY) : 0.0f; }
float Gamepad::AxisRX() const { return m_connected ? Normalize(m_state.Gamepad.sThumbRX) : 0.0f; }
float Gamepad::AxisRY() const { return m_connected ? Normalize(m_state.Gamepad.sThumbRY) : 0.0f; }

float Gamepad::Normalize(short value) const
{
return static_cast<float>(value) / kStickNorm;
}
}

