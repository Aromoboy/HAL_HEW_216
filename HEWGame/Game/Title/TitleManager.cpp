#include "TitleManager.h"

#include "Core/UpdateScheduler.h"
#include "PowerUpNode.h"
#include "SpriteRenderer.h"
#include "ICamera.h"
#include "Defines.h"
#include "Debug.h"
#include "TitleRenderer.h"
#include "DrawMatrix.h"
#include "Defines.h"

#include <cassert>
#include <utility>


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
		TitleNode(a_cache.Get("GameSceneNode"), float2(3.0f, 0.0f), float2(SCREEN_WIDTH * kTitleNodeMagnification.x, SCREEN_HEIGHT* kTitleNodeMagnification.y)),
		TitleNode(a_cache.Get("PowerUpSceneNode"), float2(-2.5f, 0.0f), float2(SCREEN_WIDTH* kTitleNodeMagnification.x, SCREEN_HEIGHT* kTitleNodeMagnification.y)),
		TitleNode(a_cache.Get("GameEndNode"), float2(-2.5f, 0.0f), float2(SCREEN_WIDTH* kTitleNodeMagnification.x, SCREEN_HEIGHT* kTitleNodeMagnification.y))
	)
	, m_selectNodes(
		TitleNode(a_cache.Get("SelectGameSceneNode"), float2(0.0f, 0.0f), float2(SCREEN_WIDTH* kTitleNodeMagnification.x, SCREEN_HEIGHT* kTitleNodeMagnification.y)),
		TitleNode(a_cache.Get("SelectPowerUpSceneNode"), float2(0.0f, 0.0f), float2(SCREEN_WIDTH* kTitleNodeMagnification.x, SCREEN_HEIGHT* kTitleNodeMagnification.y)),
		TitleNode(a_cache.Get("SelectGameEndNode"), float2(0.0f, 0.0f), float2(SCREEN_WIDTH* kTitleNodeMagnification.x, SCREEN_HEIGHT* kTitleNodeMagnification.y))
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
	for (auto it : m_nodes)
	{
		a_titleRenderer.Draw(it.nodeTexture, it.pos, it.size);
	}

	
	//a_titleRenderer.Draw(m_selectNodes.at(m_cursor->GetCursorNum()).nodeTexture, m_selectNodes.at(m_cursor->GetCursorNum()).pos, m_selectNodes.at(m_cursor->GetCursorNum()).size, camera);
	SetDepthTest(true);
}

