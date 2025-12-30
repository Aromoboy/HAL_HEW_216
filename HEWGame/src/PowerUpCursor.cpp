#include "PowerUpCursor.h"
#include "Core/Input.h"
#include "DebugConsole.h"
#include <iostream>

constexpr float maxMoveSecond = 0.10f;
constexpr float maxPowerUpSecond = 0.10f;


PowerUpCursor::PowerUpCursor(std::shared_ptr<Texture2D> a_texture, PowerUpManager* a_powerUpManager, PowerUpManager::NodeFormat a_format, int& a_cost)
	:m_powerUpManager(a_powerUpManager)
	, m_index(0)
	, m_format(a_format)
	, m_cost(a_cost)
	, m_texture(a_texture)
	, m_moveTimer(0.0f)
	, m_keyTimer(0.0f)
{
}

void PowerUpCursor::OnUpdate(Core::UpdateContext& ctx)
{
	(void)ctx;

	m_moveTimer -= ctx.dt;
	m_keyTimer -= ctx.dt;


	if (m_moveTimer <= 0.0f)
	{
		m_moveTimer = maxMoveSecond;
		MoveInput(ctx);
	}
	
	
	if (m_keyTimer <= 0.0f && ctx.input.GetAxis(Core::ActionId::Confirm))
	{
		m_keyTimer = maxPowerUpSecond;
		std::optional<int> result = m_powerUpManager->GetCost(m_index);
		if (result != std::nullopt && result <= m_cost)
		{
			if (m_powerUpManager->LevelUp(m_index)) m_cost -= result.value();
		}

		
	}

}



void PowerUpCursor::Draw(PowerUpRenderer& render, const CameraSpace::ICamera& camera)
{

	std::optional<float2> result = m_powerUpManager->GetDrawPosition(m_index);
	
	float2 drawPosition = result ? result.value() : float2{ 0.0f, 0.0f };
	
	result = m_powerUpManager->GetDrawScale(m_index);

	float2 drawScale = result ? result.value() : float2{ 1.0f, 1.0f };
	
	
	render.DrawCursor(m_texture, drawPosition, drawScale, camera);
}


float2 PowerUpCursor::GetDrawPos()
{
	std::optional<float2> result = m_powerUpManager->GetDrawPosition(m_index);

	float2 drawPosition = result ? result.value() : float2{ 0.0f, 0.0f };

	return float2{ drawPosition.x, drawPosition.y};
}



void PowerUpCursor::MoveInput(Core::UpdateContext& ctx)
{
	float controllerX = ctx.input.GetAxis(Core::ActionId::MoveX);
	float controllerY = ctx.input.GetAxis(Core::ActionId::RotateYaw);

	if (controllerX < 0 || ctx.input.IsPress('A'))
	{
		if (m_index - m_format.y >= 0) m_index -= m_format.y;
	}
	else if (controllerX > 0 || ctx.input.IsPress('D'))
	{
		if (m_index + m_format.y < m_powerUpManager->GetNodeCount()) m_index += m_format.y;
	}

	if (controllerY > 0 || ctx.input.IsPress('W'))
	{
		if (m_index - 1 >= 0 && m_index % m_format.y != 0) m_index--;
	}
	else if (controllerY < 0 || ctx.input.IsPress('S'))
	{
		if (m_index + 1 < m_powerUpManager->GetNodeCount() && (m_index + 1) % m_format.y != 0)	m_index++;
	}

}

float3 GetDrawPos()
{
	return float3();
}
