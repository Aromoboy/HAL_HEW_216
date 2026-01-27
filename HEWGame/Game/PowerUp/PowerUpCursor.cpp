#include "PowerUpCursor.h"
#include "Core/Input.h"
#include "DebugConsole.h"
#include "Defines.h"
#include <iostream>

constexpr float maxMoveSecond = 0.10f;
constexpr float maxPowerUpSecond = 0.10f;
constexpr float2 kCursorOffset = { 0.0015f, 0.014f };
constexpr float kPowerUpInputDeadzone = 0.2f;

PowerUpCursor::PowerUpCursor(std::shared_ptr<Texture2D> a_texture, PowerUpManager* a_powerUpManager, PowerUpManager::NodeFormat a_format, int& a_cost, Core::Input& input, EffectSystem& a_effectSystem, std::function<void(int)> onPurchase)
	:m_powerUpManager(a_powerUpManager)
	, m_input(&input)
	, m_index(0)
	, m_format(a_format)
	, m_cost(a_cost)
	, m_texture(a_texture)
	, m_moveTimer(0.0f)
	, m_keyTimer(0.0f)
	, m_flicker(0.0f)
	, m_alpha(0.0f)
	, m_effectSystem(a_effectSystem)
	, m_onPurchase(std::move(onPurchase))
{
}

void PowerUpCursor::OnUpdate(const Core::UpdateContext& ctx)
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
	
	
	if (m_input && m_keyTimer <= 0.0f && m_input->IsTrigger(Core::ActionId::Select))
	{
		m_keyTimer = maxPowerUpSecond;
		std::optional<int> result = m_powerUpManager->GetCost(m_index);
		if (result != std::nullopt && result <= m_cost)
		{
			if (m_powerUpManager->LevelUp(m_index))
			{
				m_cost -= result.value();
				if (m_onPurchase)
				{
					m_onPurchase(m_index);
				}
			}


			std::optional<float2> result = m_powerUpManager->GetDrawPosition(m_index);

			float2 drawPosition = result ? result.value() : float2{ 0.0f, 0.0f };

			drawPosition.x += kCursorOffset.x;
			drawPosition.y += kCursorOffset.y;


			EffectSystem::EffectCreateRequest request("PowerUp", float3(drawPosition.x, drawPosition.y, 1.0f), float3(0.0f, 0.0f, 0.0f));
			m_effectSystem.SpawnEffect(request);
		}

		
	}

}



void PowerUpCursor::Draw(PowerUpRenderer& render, const CameraSpace::ICamera& camera)
{

	std::optional<float2> result = m_powerUpManager->GetDrawPosition(m_index);

	float2 drawPosition = result ? result.value() : float2{ 0.0f, 0.0f };
	
	drawPosition.x += kCursorOffset.x;
	drawPosition.y += kCursorOffset.y;

	result = m_powerUpManager->GetDrawScale(m_index);

	float2 drawScale = result ? result.value() : float2{ 1.0f, 1.0f };
	
	
	render.DrawCursor(m_texture, drawPosition, drawScale, camera, m_alpha);
}


float2 PowerUpCursor::GetDrawPos()
{
	std::optional<float2> result = m_powerUpManager->GetDrawPosition(m_index);

	float2 drawPosition = result ? result.value() : float2{ 0.0f, 0.0f };

	return float2{ drawPosition.x, drawPosition.y};
}



void PowerUpCursor::MoveInput(Core::Input& input)
{
	float controllerX = input.GetAxis(Core::ActionId::MoveX);
	float controllerY = input.GetAxis(Core::ActionId::RotateYaw);

	if (controllerX < -kPowerUpInputDeadzone || input.IsPress('A'))
	{
		if (m_index - m_format.y >= 0) m_index -= m_format.y;
	}
	else if (controllerX > kPowerUpInputDeadzone || input.IsPress('D'))
	{
		if (m_index + m_format.y < m_powerUpManager->GetNodeCount()) m_index += m_format.y;
	}

	if (controllerY > kPowerUpInputDeadzone || input.IsPress('W'))
	{
		if (m_index - 1 >= 0 && m_index % m_format.y != 0) m_index--;
	}
	else if (controllerY < -kPowerUpInputDeadzone || input.IsPress('S'))
	{
		if (m_index + 1 < m_powerUpManager->GetNodeCount() && (m_index + 1) % m_format.y != 0)	m_index++;
	}

}

float3 GetDrawPos()
{
	return float3();
}
