
/**
* @file Game.h
* @brief テスト画面表示
* @author 越智晴彦
* @date 2025/10/15
* @details 一応メインゲームとしての形で作ってある
*/

#ifndef ___GAME_H___
#define ___GAME_H___


#include "SpriteDrawer.h"
#include "DirectX.h"


class Game
{
public:
	Game();

	void UpdateGame();

	void DrawGame();


private:

	ID3D11ShaderResourceView* m_pTexture;

};

#endif // !___GAME_H___