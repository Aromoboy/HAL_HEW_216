#include "Game.h"
#include "SpriteDrawer.h"
#include "VertexBuffer.h"
#include "DirectXTex/TextureLoad.h"

//baka
int y;

Game::Game()
{
	HRESULT hr = LoadTextureFromFile(GetDevice(), "rom/HelloWorld.png", &m_pTexture);
	if (FAILED(hr))
	{
		MessageBox(NULL, "画像ファイルの読み込みに失敗しました。", "Error", MB_OK);
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
