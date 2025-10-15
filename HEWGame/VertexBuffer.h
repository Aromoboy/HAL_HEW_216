#ifndef ___VERTEX_BUFFER_H___
#define ___VERTEX_BUFFER_H___

#include "SpriteDrawer.h"
#include "DirectX.h"

// @brief 頂点バッファの作成
// @param[in] vtxData 頂点データの配列
// @param[in] vtxNum 頂点の数
ID3D11Buffer* CreateVertexBuffer(Vertex* vtxData, UINT vtxNum);

void InitVertex();

void UninitVertex();

ID3D11Buffer* GetUnitVertexBuffer();


#endif