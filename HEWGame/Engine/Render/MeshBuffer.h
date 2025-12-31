#ifndef __MESH_BUFFER_H__
#define __MESH_BUFFER_H__

#include "DirectX.h"
#include "Core/VertexBuffer.h"
#include "Core/IndexBuffer.h"

class MeshBuffer
{
public:
	struct Description
	{
		const void* pVtx;
		UINT vtxSize;
		UINT vtxCount;
		bool isWrite;
		const void* pIdx;
		UINT idxSize;
		UINT idxCount;
		D3D11_PRIMITIVE_TOPOLOGY topology;
	};
public:
	MeshBuffer();
	~MeshBuffer();

	HRESULT Create(const Description& desc);
	void Draw(int count = 0);
	HRESULT Write(void* pVtx);

	Description GetDesc();

private:
        HRESULT CreateVertexBuffer(const void* pVtx, UINT size, UINT count, bool isWrite);
        HRESULT CreateIndexBuffer(const void* pIdx, UINT size, UINT count);

private:
        Core::VertexBuffer m_vertexBuffer;
        Core::IndexBuffer m_indexBuffer;
        Description m_desc;

};

#endif
