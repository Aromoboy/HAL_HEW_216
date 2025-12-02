#include "MeshDX.h"

#include "Debug.h"

MeshDX::MeshDX(const MeshDX::Description& desc, unsigned long materialID)
        : m_vertexBuffer()
        , m_indexBuffer()
        , m_hasIndexBuffer(false)
{
        HRESULT hr;

        // 各種ステータスを設定
        m_indexCount = desc.idxCount;

        m_materialID = materialID;

        // 頂点バッファ作成
        hr = CreateVertexBuffer(desc.pVtx, desc.vtxSize, desc.vtxCount);
        if (FAILED(hr))
        {
#ifdef _DEBUG
                DebugMessage("頂点バッファの作成に失敗しました");
#endif
                return;
        }
        // インデックスバッファ作成
        if (desc.pIdx) {
                hr = CreateIndexBuffer(desc.pIdx, desc.idxSize, desc.idxCount);
                if (FAILED(hr))
                {
#ifdef _DEBUG
                        DebugMessage("インデックスバッファの作成に失敗しました");
#endif

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
        switch (size)
        {
        default:
                return E_FAIL;
        case 2:
        case 4:
                break;
        }

        HRESULT hr = m_indexBuffer.Init(pIdx, size, count);
        if (SUCCEEDED(hr))
        {
                m_hasIndexBuffer = true;
        }

        return hr;
}
