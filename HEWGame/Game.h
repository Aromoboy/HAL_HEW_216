/**
* @file Game.h
* @author 越智晴彦
* @date 2025/10/15
* @details 一応メインゲームとしての形で作ってある
*/

/*
@update author ドロスミハイル
@date 2025/10/21
@details クラス・インターフェイスでGameデストラクタを宣言し、m_pTextureをnullptrにデフォルト初期化
*/

#ifndef ___GAME_H___
#define ___GAME_H___


#include "SpriteDrawer.h"
#include "DirectX.h"


class Game
{
public:
    Game();

    ~Game();

    void UpdateGame();

    void DrawGame();


private:

    ID3D11ShaderResourceView* m_pTexture = nullptr;

};

#endif // !___GAME_H___