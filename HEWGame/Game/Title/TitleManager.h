#pragma once

#pragma once

#include "Core/UpdateNode.h"

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <utility>
#include <optional>
#include <array>
#include "TextureCache.h"
#include "Vector.h"
#include "TitleRenderer.h"
#include "TitleCursor.h"

namespace Core
{
	class UpdateScheduler;
}

namespace Systems
{
	class DrawSystem;
	class PhysicsMachine;
}

class SpriteRenderer;

namespace CameraSpace
{
	class ICamera;
}


enum TitleNodeType
{
	TITLE_GAME,
	TITLE_POWERUP,
	TITLE_END,
	TITLE_MAX_NODE
};

struct TitleNode
{
	std::shared_ptr<Texture2D> nodeTexture;
	float2 pos;
	float2 size;

	TitleNode(std::shared_ptr<Texture2D> a_nodeTexture,
	float2 a_pos,
	float2 a_size)
		: nodeTexture(a_nodeTexture)
		, pos(a_pos)
		, size(a_size)
	{
	}

};

class TitleManager : public Core::UpdateNode
{
public:

	struct DrawInfomation
	{
		float3 drawPos;
		float3 drawScale;

		DrawInfomation(float3 _drawPos, float3 _drawScale)
			: drawPos(_drawPos)
			, drawScale(_drawScale)
		{
		}
	};

	TitleManager(Core::UpdateScheduler* a_scheduler, TextureCache& cache, const TitleCursor::NodeFormat& format, Core::Input& a_input);
	~TitleManager();

	void RegisterCursor();
	void UnregisterCursor();

	void OnUpdate(const Core::UpdateContext& ctx) final;

	void SetFormat(const TitleCursor::NodeFormat&);

	bool m_cursorRegistered = false;
	// 全てのノードとカーソルを描画
	void DrawAll(TitleRenderer& TitleRenderer);


private:
	Core::UpdateScheduler* m_scheduler;

	TitleCursor::NodeFormat m_format;

	std::array<TitleNode, TITLE_MAX_NODE> m_nodes;
	std::array<TitleNode, TITLE_MAX_NODE> m_selectNodes;

	std::unique_ptr<TitleCursor> m_cursor;

	TextureCache& m_cache;
};
