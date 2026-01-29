#include "TitleCursor.h"
#include "Core/Input.h"
#include "DebugConsole.h"
#include "Defines.h"
#include <iostream>

constexpr float maxMoveSecond = 0.10f;
constexpr float maxPowerUpSecond = 0.10f;
constexpr float2 kCursorOffset = { 0.0015f, 0.014f };
constexpr float2 kTitleCursorDeadZone(0.3f);

TitleCursor::TitleCursor(std::shared_ptr<Texture2D> a_texture, TitleCursor::NodeFormat a_format, Core::Input& input)
	: m_input(&input)
	, m_index(0)
	, m_format(a_format)
	, m_moveTimer(0.0f)
	, m_keyTimer(0.0f)
	, m_flicker(0.0f)
	, m_alpha(0.0f)
	, m_isClick(false)
{
}

void TitleCursor::OnUpdate(const Core::UpdateContext& ctx)
{
	(void)ctx;

	m_moveTimer -= ctx.dt;
	m_keyTimer -= ctx.dt;
	m_flicker = std::fmod(m_flicker + ctx.dt, 2 * PI);

	float sinAlpha = std::sin(m_flicker);

	m_alpha = std::abs(sinAlpha) * 0.5f + 0.5f;


	if (m_moveTimer <= 0.0f)
	{
		m_moveTimer = maxMoveSecond;
		if (m_input)
		{
			MoveInput(*m_input);
		}
	}

	m_isClick = m_input && 
		m_keyTimer <= 0.0f && 
		m_input->IsTrigger(Core::ActionId::Select);

}

CursorIndex TitleCursor::GetCursorNum()
{
	return m_index;
}

bool TitleCursor::IsCursorClick()
{
	return m_isClick;
}

void TitleCursor::MoveInput(Core::Input& input)
{
	float controllerX = input.GetAxis(Core::ActionId::MoveX);
	float controllerY = input.GetAxis(Core::ActionId::RotateYaw);

	if (controllerX < -kTitleCursorDeadZone.x || input.IsPress('A'))
	{
		m_index = 2;
	}
	else if (controllerX > kTitleCursorDeadZone.x || input.IsPress('D'))
	{
		m_index = 0;
	}

	if (controllerY > kTitleCursorDeadZone.y || input.IsPress('W'))
	{
		m_index = 2;
	}
	else if (controllerY < -kTitleCursorDeadZone.y || input.IsPress('S'))
	{
		m_index = 1;
	}

}

void TitleCursor::Draw(PowerUpRenderer& render, const CameraSpace::ICamera& camera, float2 pos)
{
	float2 drawScale = float2{ 1.0f, 1.0f };

	render.DrawCursor(m_texture, pos, drawScale, camera, m_alpha);
}
