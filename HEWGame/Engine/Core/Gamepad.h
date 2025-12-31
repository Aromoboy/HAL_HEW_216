// ゲームパッド入力の状態を取得する
// 入力更新や操作判定から参照される

#pragma once

#include <Windows.h>
#include <Xinput.h>

namespace Core
{
class Gamepad
{
public:
Gamepad();

void Poll(int index = 0);

bool IsConnected() const;
bool IsButtonDown(WORD button) const;
bool WasButtonDown(WORD button) const;

float AxisLX() const;
float AxisLY() const;
float AxisRX() const;
float AxisRY() const;

const XINPUT_STATE& GetState() const { return m_state; }
const XINPUT_STATE& GetPreviousState() const { return m_prevState; }

private:
float Normalize(short value) const;

XINPUT_STATE m_state;
XINPUT_STATE m_prevState;
bool m_connected;
};
}

