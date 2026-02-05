#pragma once

#include "Core/UpdateNode.h"
#include "PowerUpManager.h"
#include "PowerUpRenderer.h"
#include "TextureDX.h"
#include <functional>
#include <memory>
#include "EffectSystem.h"

using CursorIndex = int;

namespace Core
{
	class Input;
}

class PowerUpCursor : public Core::UpdateNode
{
public:
	PowerUpCursor(std::shared_ptr<Texture2D> a_texture, PowerUpManager* a_powerUpManager, PowerUpManager::NodeFormat a_format, int& a_cost, Core::Input& input, EffectSystem& a_effectSystem, std::function<void(int)> onPurchase);

	void OnUpdate(const Core::UpdateContext& ctx) final;

	void Draw(PowerUpRenderer& render, const CameraSpace::ICamera& camera);

	float2 GetDrawPos();

private:
	PowerUpManager* m_powerUpManager;
	Core::Input* m_input;
	CursorIndex m_index;
	PowerUpManager::NodeFormat m_format;
	std::shared_ptr<Texture2D> m_texture;
	float m_keyTimer;
	int& m_cost;
	float m_flicker;
	float m_alpha;
	bool m_prevLeft;
	bool m_prevRight;
	bool m_prevUp;
	bool m_prevDown;
	EffectSystem& m_effectSystem;
	std::function<void(int)> m_onPurchase;

	void MoveInput(Core::Input& input);
};
