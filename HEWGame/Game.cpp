#include "Game.h"
#include "SpriteDrawer.h"
#include "VertexBuffer.h"
#include "DirectXTex/TextureLoad.h"
#include <Windows.h>
#include <cstdio>

Game::Game()
{
	HRESULT hr = LoadTextureFromFile(GetDevice(), "rom/HelloWorld.png", &m_pTexture);
	if (FAILED(hr))
	{
		char errorMessage[256];
		sprintf_s(errorMessage, "摜t@C̓ǂݍ݂Ɏs܂B(HRESULT=0x%08lX)", hr);
		MessageBoxA(NULL, errorMessage, "Error", MB_OK | MB_ICONERROR);
		OutputDebugStringA(errorMessage);
		OutputDebugStringA("\n");
		m_pTexture = nullptr;
	}
}

Game::~Game()
{
	if (m_pTexture != nullptr)
	{
		m_pTexture->Release();
		m_pTexture = nullptr;
	}
}

void Game::UpdateGame()
{
}

void Game::DrawGame()
{
	// 最低限の描画処理
	SetSpriteScale(500.0f, 500.0f);
	SetSpriteTexture(m_pTexture);

	DrawSprite(GetUnitVertexBuffer());

}
