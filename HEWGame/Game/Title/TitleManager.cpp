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
constexpr float2 kTitleBackMagnification = { 0.0115f, 0.0115f };
constexpr float2 kTitleNodeMagnification = { 0.01f, 0.01f };

constexpr float2 kTitleStartNodeOffset	 = {  3.0f,  0.0f };
constexpr float2 kTitlePowerUpNodeOffset = { -2.1f, -0.1f };
constexpr float2 kTitleEndNodeOffset	 = { -1.7f,  0.1f };

constexpr float2 kSelectStartNodeOffset		= { 0.0f, -0.15f };
constexpr float2 kSelectPowerUpNodeOffset	= { 0.275f, -0.105f };
constexpr float2 kSelectEndNodeOffset		= { 0.0f, 0.0f };

TitleManager::TitleManager(Core::UpdateScheduler* a_scheduler, TextureCache& a_cache, const TitleCursor::NodeFormat& a_format, Core::Input& a_input)
	:m_scheduler(a_scheduler)
	, m_cache(a_cache)
	, m_format(a_format)
	, m_gameEndRequest(false)
	, m_sceneRequest(SCENE_REQUEST_NONE)
	, m_cursor(std::make_unique<TitleCursor>(
		a_cache.Get("TitleCursor"),
		a_format,
		a_input))
	, m_nodes(
		TitleNode(a_cache.Get("GameSceneNode"), kTitleStartNodeOffset, float2(SCREEN_WIDTH * kTitleNodeMagnification.x, SCREEN_HEIGHT* kTitleNodeMagnification.y)),
		TitleNode(a_cache.Get("PowerUpSceneNode"), kTitlePowerUpNodeOffset, float2(SCREEN_WIDTH* kTitleNodeMagnification.x, SCREEN_HEIGHT* kTitleNodeMagnification.y)),
		TitleNode(a_cache.Get("GameEndNode"), kTitleEndNodeOffset, float2(SCREEN_WIDTH* kTitleNodeMagnification.x, SCREEN_HEIGHT* kTitleNodeMagnification.y))
	)
	, m_selectNodes(
		TitleNode(a_cache.Get("SelectGameSceneNode"), kTitleStartNodeOffset + kSelectStartNodeOffset, float2(SCREEN_WIDTH* kTitleNodeMagnification.x, SCREEN_HEIGHT* kTitleNodeMagnification.y)),
		TitleNode(a_cache.Get("SelectPowerUpSceneNode"), kTitlePowerUpNodeOffset + kSelectPowerUpNodeOffset, float2(SCREEN_WIDTH* kTitleNodeMagnification.x, SCREEN_HEIGHT* kTitleNodeMagnification.y)),
		TitleNode(a_cache.Get("SelectGameEndNode"), kTitleEndNodeOffset + kSelectEndNodeOffset, float2(SCREEN_WIDTH* kTitleNodeMagnification.x, SCREEN_HEIGHT* kTitleNodeMagnification.y))
	)
	, m_backTexture(a_cache.Get("TitleBack"))
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

SceneMoveRequest TitleManager::GetSceneMoveRequest()
{
	return m_sceneRequest;
}

bool TitleManager::GetGameEndRequest()
{
	return m_gameEndRequest;
}

void TitleManager::OnUpdate(const Core::UpdateContext& ctx)
{
	(void)ctx;

	if (m_cursor->IsCursorClick())
	{
		switch (m_cursor->GetCursorNum())
		{
		case TITLE_GAME:
			m_sceneRequest = GAME_REQUEST;
			break;

		case TITLE_POWERUP:
			m_sceneRequest = POWERUP_REQUEST;
			break;

		case TITLE_END:
			m_gameEndRequest = true;
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
	a_titleRenderer.Draw(m_backTexture, float2(0.0f, 0.0f), float2(SCREEN_WIDTH * kTitleBackMagnification.x, SCREEN_HEIGHT * kTitleBackMagnification.y));

	for (auto it : m_nodes)
	{
		a_titleRenderer.Draw(it.nodeTexture, it.pos, it.size);
	}

	
	a_titleRenderer.Draw(m_selectNodes.at(m_cursor->GetCursorNum()).nodeTexture, m_selectNodes.at(m_cursor->GetCursorNum()).pos, m_selectNodes.at(m_cursor->GetCursorNum()).size);
	SetDepthTest(true);
}

