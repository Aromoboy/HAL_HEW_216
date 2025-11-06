#include "Game.h"

#include <windows.h>

#include "SpriteDrawer.h"
#include "DirectXTex/TextureLoad.h"
#include "VertexBuffer.h"

Game::Game(ID3D11Device* device)
        : m_pTexture(nullptr)
{
        HRESULT hr = LoadTextureFromFile(device, "rom/HelloWorld.png", &m_pTexture);
        if (FAILED(hr))
        {
                MessageBox(NULL, "摜t@C̓ǂݍ݂Ɏs܂B", "Error", MB_OK);
                m_pTexture = nullptr;
        }
}

void Game::UpdateGame()
{
}

void Game::DrawGame()
{
        // Œ̕`揈
        SetSpriteScale(500.0f, 500.0f);
        SetSpriteTexture(m_pTexture);

        DrawSprite(GetUnitVertexBuffer());
}

