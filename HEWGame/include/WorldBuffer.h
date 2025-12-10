#ifndef __WORLD_BUFFER_H__
#define __WORLD_BUFFER_H__

#include <DirectXMath.h>

struct WorldBuffer
{
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT4X4 view;
        DirectX::XMFLOAT4X4 proj;
        DirectX::XMFLOAT4 normalBasis[3];
};

#endif // __WORLD_BUFFER_H__
