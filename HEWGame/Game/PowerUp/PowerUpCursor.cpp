#include "PowerUpCursor.h"
#include "Core/Input.h"
#include "DebugConsole.h"
#include "Defines.h"
#include "SoundManagerEX.h"
#include <iostream>

constexpr float maxMoveSecond = 0.10f;
constexpr float maxPowerUpSecond = 0.10f;
constexpr float2 kCursorOffset = { 0.0015f, 0.014f };
constexpr float kPowerUpInputDeadzone = 0.6f;

PowerUpCursor::PowerUpCursor(std::shared_ptr<Texture2D> a_texture, PowerUpManager* a_powerUpManager, PowerUpManager::NodeFormat a_format, int& a_cost, Core::Input& input, EffectSystem& a_effectSystem, std::function<void(int)> onPurchase)
	:m_powerUpManager(a_powerUpManager)
	, m_input(&input)
	, m_index(0)
	, m_format(a_format)
	, m_cost(a_cost)
	, m_texture(a_texture)
	, m_keyTimer(0.0f)
	, m_prevLeft(false)
	, m_prevRight(false)
	, m_prevUp(false)
	, m_prevDown(false)
	, m_flicker(0.0f)
	, m_alpha(0.0f)
	, m_effectSystem(a_effectSystem)
	, m_onPurchase(std::move(onPurchase))
{
}

void PowerUpCursor::OnUpdate(const Core::UpdateContext& ctx)
{
	(void)ctx;

	m_keyTimer -= ctx.dt;
	m_flicker = std::fmod(m_flicker + ctx.dt, 2 * PI);

	float sinAlpha = std::sin(m_flicker);

	m_alpha = std::abs(sinAlpha) * 0.5f + 0.5f;


		if (m_input)
		{
			MoveInput(*m_input);
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

	bool left = (controllerX < -kPowerUpInputDeadzone) || input.IsTrigger('A');
	bool right = (controllerX > kPowerUpInputDeadzone) || input.IsTrigger('D');
	bool up = (controllerY > kPowerUpInputDeadzone) || input.IsTrigger('W');
	bool down = (controllerY < -kPowerUpInputDeadzone) || input.IsTrigger('S');

	bool moved = false;

	if (left && !m_prevLeft)
	{
		if (m_index - m_format.y >= 0) { m_index -= m_format.y; moved = true; }
	}
	else if (right && !m_prevRight)
	{
		if (m_index + m_format.y < m_powerUpManager->GetNodeCount()) { m_index += m_format.y; moved = true; }
	}

	if (up && !m_prevUp)
	{
		if (m_index - 1 >= 0 && m_index % m_format.y != 0) { m_index--; moved = true; }
	}
	else if (down && !m_prevDown)
	{
		if (m_index + 1 < m_powerUpManager->GetNodeCount() && (m_index + 1) % m_format.y != 0) { m_index++; moved = true; }
	}

	m_prevLeft = left;
	m_prevRight = right;
	m_prevUp = up;
	m_prevDown = down;

	if (moved)
	{
		if (auto* soundMgr = SoundManagerEX::GetInstance())
		{
			soundMgr->PlaySoundEX("Assets/Audio/SE_Enter.wav", false, 0.5f);
		}
	}
}



float3 GetDrawPos()
{
	return float3();
}
