#include "MeshDX.h"

#include "Debug.h"

MeshDX::MeshDX(const MeshDX::Description& desc, unsigned long materialID)
        : m_vertexBuffer()
        , m_indexBuffer()
        , m_hasIndexBuffer(false)
        , m_topology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
{
        HRESULT hr;

        // 各種ステータスを設定
        m_indexCount = desc.idxCount;
        m_materialID = materialID;

        // 頂点バッファ作成
        const UINT vertexStride = sizeof(MeshDX::Vertex);
        if (desc.vtxSize != vertexStride)
        {
                DebugMessage("MeshDX received unexpected vertex size.\n");
                return;
        }

        hr = CreateVertexBuffer(desc.pVtx, vertexStride, desc.vtxCount);
        if (FAILED(hr))
        {
#ifdef _DEBUG
                DebugMessage("頂点バッファの作成に失敗しました");
#endif
                return;
        }

        // インデックスバッファ作成
        if (desc.pIdx)
        {
                hr = CreateIndexBuffer(desc.pIdx, desc.idxSize, desc.idxCount);
                if (FAILED(hr))
                {
                        DebugMessage("インデックスバッファの作成に失敗しました");
                        return;
                }
        }
}

MeshDX::~MeshDX()
{
        m_vertexBuffer.Shutdown();
        m_indexBuffer.Shutdown();
}


// 各種ゲッタ
unsigned int MeshDX::GetMaterialID()
{
        return m_materialID;
}

const Core::VertexBuffer& MeshDX::GetVertexBuffer() const
{
        return m_vertexBuffer;
}

const Core::IndexBuffer* MeshDX::GetIndexBuffer() const
{
        return m_hasIndexBuffer ? &m_indexBuffer : nullptr;
}

bool MeshDX::HasIndexBuffer() const
{
        return m_hasIndexBuffer;
}

unsigned int MeshDX::GetIndexCount()
{
        return m_indexCount;
}


// 頂点バッファの作成
HRESULT MeshDX::CreateVertexBuffer(const void* pVtx, UINT size, UINT count)
{
        return m_vertexBuffer.Init(pVtx, size, count, false);
}

// 頂点バッファの作成
HRESULT MeshDX::CreateIndexBuffer(const void* pIdx, UINT size, UINT count)
{
        // インデックスサイズの確認
        if (size != sizeof(unsigned int))
        {
                DebugMessage("MeshDX only supports 32-bit indices.\n");
                return E_FAIL;
        }

        HRESULT hr = m_indexBuffer.Init(pIdx, size, count);
        if (SUCCEEDED(hr))
        {
                m_hasIndexBuffer = true;
        }

        return hr;
}
