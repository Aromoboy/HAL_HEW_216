#ifndef ___VERTEX_BUFFER_H___
#define ___VERTEX_BUFFER_H___

#include <d3d11.h>

#include "SpriteDrawer.h"

// @brief _obt@̍쐬
// @param[in] device Direct3D device
// @param[in] vtxData _f[^̔z
// @param[in] vtxNum _̐
ID3D11Buffer* CreateVertexBuffer(ID3D11Device* device, Vertex* vtxData, UINT vtxNum);

void InitVertex(ID3D11Device* device);

void UninitVertex();

ID3D11Buffer* GetUnitVertexBuffer();

#endif

