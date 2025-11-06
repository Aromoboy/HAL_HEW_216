/**
* @file Game.h
* @brief eXgʕ\
* @author zqF
* @date 2025/10/15
* @details ꉞCQ[ƂĂ̌`ōĂ
*/

#ifndef ___GAME_H___
#define ___GAME_H___

#include <d3d11.h>

#include "SpriteDrawer.h"

class Game
{
public:
        explicit Game(ID3D11Device* device);

        void UpdateGame();

        void DrawGame();

private:
        ID3D11ShaderResourceView* m_pTexture;
};

#endif // !___GAME_H___

