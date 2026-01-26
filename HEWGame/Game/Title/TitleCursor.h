#pragma once

#include "Core/UpdateNode.h"
#include "PowerUpManager.h"
#include "PowerUpRenderer.h"
#include "TextureDX.h"
#include <memory>
#include "EffectSystem.h"

using CursorIndex = int;

namespace Core
{
	class Input;
}

class TitleCursor : public Core::UpdateNode
{
public:
	struct NodeFormat
	{
		int x;
		int y;
		int maxNode;

		NodeFormat(int a_x, int a_y, int a_maxNode)
			:x(a_x)
			, y(a_y)
			, maxNode(a_maxNode)
		{
		}
	};


	TitleCursor(std::shared_ptr<Texture2D> a_texture, TitleCursor::NodeFormat a_format, Core::Input& input);

	void OnUpdate(const Core::UpdateContext& ctx) override;
	void Draw(PowerUpRenderer& render, const CameraSpace::ICamera& camera, float2 pos);

	CursorIndex GetCursorNum();
	bool IsCursorClick();

private:
	Core::Input* m_input;
	CursorIndex m_index;
	TitleCursor::NodeFormat m_format;
	std::shared_ptr<Texture2D> m_texture;
	float m_moveTimer;
	float m_keyTimer;
	float m_flicker;
	float m_alpha;
	bool m_isClick;

	void MoveInput(Core::Input& input);
};
