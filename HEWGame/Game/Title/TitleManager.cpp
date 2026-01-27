#include "TitleManager.h"

#include "Core/UpdateScheduler.h"
#include "PowerUpNode.h"
#include "SpriteRenderer.h"
#include "ICamera.h"
#include "AppGame.h"
#include "Defines.h"
#include "Debug.h"
#include "TitleRenderer.h"
#include "DrawMatrix.h"
#include <cassert>
#include <utility>

namespace
{
	float2 ResolveTitleNodeSize()
	{
		const Core::DXContext& dxContext = App::Game::Instance().GetDXContext();
		const float width = static_cast<float>(dxContext.GetWidth());
		const float height = static_cast<float>(dxContext.GetHeight());
		const float fallbackWidth = static_cast<float>(SCREEN_WIDTH);
		const float fallbackHeight = static_cast<float>(SCREEN_HEIGHT);

		const float resolvedWidth = width > 0.0f ? width : fallbackWidth;
		const float resolvedHeight = height > 0.0f ? height : fallbackHeight;

		return float2(resolvedWidth * kTitleNodeMagnification.x,
			resolvedHeight * kTitleNodeMagnification.y);
	}
}

constexpr float2 kUnitDistanse = { 1.4f, -0.5f };
constexpr float2 kTitleNodeMagnification = { 0.01f, 0.01f };

TitleManager::TitleManager(Core::UpdateScheduler* a_scheduler, TextureCache& a_cache, const TitleCursor::NodeFormat& a_format, Core::Input& a_input)
	:m_scheduler(a_scheduler)
	, m_cache(a_cache)
	, m_format(a_format)
	, m_cursor(std::make_unique<TitleCursor>(
		a_cache.Get("TitleCursor"),
		a_format,
		a_input))
	, m_nodes(
		TitleNode(a_cache.Get("GameSceneNode"), float2(3.0f, 0.0f), ResolveTitleNodeSize()),
		TitleNode(a_cache.Get("PowerUpSceneNode"), float2(-2.5f, 0.0f), ResolveTitleNodeSize()),
		TitleNode(a_cache.Get("GameEndNode"), float2(-2.5f, 0.0f), ResolveTitleNodeSize())
	)
	, m_selectNodes(
		TitleNode(a_cache.Get("SelectGameSceneNode"), float2(0.0f, 0.0f), ResolveTitleNodeSize()),
		TitleNode(a_cache.Get("SelectPowerUpSceneNode"), float2(0.0f, 0.0f), ResolveTitleNodeSize()),
		TitleNode(a_cache.Get("SelectGameEndNode"), float2(0.0f, 0.0f), ResolveTitleNodeSize())
	)
{
}

TitleManager::~TitleManager()
{
}

void TitleManager::RegisterCursor()
{
	if (!m_scheduler || !m_cursor || m_cursorRegistered)
	{
		return;
	}

	auto handle = m_scheduler->Register(*m_cursor);
	if (handle.IsValid())
	{
		m_cursor->SetRegistrationHandle(std::move(handle));
		m_cursorRegistered = true;
	}
}

void TitleManager::UnregisterCursor()
{
	if (!m_cursor || !m_cursorRegistered)
	{
		return;
	}

	m_cursor->ReleaseRegistrationHandle();
	m_cursorRegistered = false;
}

void TitleManager::OnUpdate(const Core::UpdateContext& ctx)
{
	(void)ctx;

	if (m_cursor->IsCursorClick())
	{
		switch (m_cursor->GetCursorNum())
		{
		case TITLE_GAME:
			
			break;

		case TITLE_POWERUP:

			break;

		case TITLE_END:

			break;
		}
	}
}

void TitleManager::SetFormat(const TitleCursor::NodeFormat& a_format)
{
	m_format = a_format;
}


void TitleManager::DrawAll(TitleRenderer& a_titleRenderer)
{
	SetDepthTest(false);
	const float2 nodeSize = ResolveTitleNodeSize();
	for (auto& node : m_nodes)
	{
		node.size = nodeSize;
		a_titleRenderer.Draw(node.nodeTexture, node.pos, node.size);
	}

	for (auto& node : m_selectNodes)
	{
		node.size = nodeSize;
	}

	//a_titleRenderer.Draw(m_selectNodes.at(m_cursor->GetCursorNum()).nodeTexture, m_selectNodes.at(m_cursor->GetCursorNum()).pos, m_selectNodes.at(m_cursor->GetCursorNum()).size, camera);
	SetDepthTest(true);
}
