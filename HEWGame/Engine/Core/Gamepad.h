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

static float NormalizeStickAxis(short value, int deadZone);

const XINPUT_STATE& GetState() const { return m_state; }
const XINPUT_STATE& GetPreviousState() const { return m_prevState; }

private:
XINPUT_STATE m_state;
XINPUT_STATE m_prevState;
bool m_connected;
};
}

