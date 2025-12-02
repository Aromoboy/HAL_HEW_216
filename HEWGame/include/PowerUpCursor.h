#pragma once

#include "PowerUpCursor.h"
#include "Core/UpdateNode.h"
#include "PowerUpManager.h"
#include "SpriteRenderer.h"
#include "TextureDX.h"
#include <memory>

using CursorIndex = int;

class PowerUpCursor : public Core::UpdateNode
{
public:
	PowerUpCursor(std::shared_ptr<Texture2D> a_texture, PowerUpManager* a_powerUpManager, PowerUpManager::NodeFormat a_format, int& a_cost);

	void OnUpdate(Core::UpdateContext& ctx) final;

	void Draw(SpriteRenderer& render, const CameraSpace::ICamera& camera);

	float2 GetDrawPos();

private:
	PowerUpManager* m_powerUpManager;
	CursorIndex m_index;
	PowerUpManager::NodeFormat m_format;
	std::shared_ptr<Texture2D> m_texture;
	float m_moveTimer;
	int& m_cost;

	void MoveInput(Core::UpdateContext& ctx);
};