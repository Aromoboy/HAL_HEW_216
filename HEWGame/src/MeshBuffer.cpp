#include "MeshBuffer.h"

#include <cstring>

#include "Debug.h"

MeshBuffer::MeshBuffer()
        : m_vertexBuffer()
        , m_indexBuffer()
        , m_desc{}
{
}
MeshBuffer::~MeshBuffer()
{
        SAFE_DELETE_ARRAY(m_desc.pIdx);
        SAFE_DELETE_ARRAY(m_desc.pVtx);
        m_indexBuffer.Shutdown();
        m_vertexBuffer.Shutdown();
}

HRESULT MeshBuffer::Create(const Description& desc)
{
        HRESULT hr = E_FAIL;

        // 頂点バッファ作成
        hr = CreateVertexBuffer(desc.pVtx, desc.vtxSize, desc.vtxCount, desc.isWrite);
        if (FAILED(hr)) { return hr; }

        // インデックスバッファ作成
        if (desc.pIdx) {
                hr = CreateIndexBuffer(desc.pIdx, desc.idxSize, desc.idxCount);
                if (FAILED(hr)) { return hr; }
        }

        // バッファのコピー
        m_desc = desc;

        rsize_t vtxMemSize = desc.vtxSize * desc.vtxCount;
        void* pVtx = new char[vtxMemSize];
        memcpy_s(pVtx, vtxMemSize, desc.pVtx, vtxMemSize);
        m_desc.pVtx = pVtx;
        if (m_desc.pIdx) {
                rsize_t idxMemSize = desc.idxSize * desc.idxCount;
                void* pIdx = new char[idxMemSize];
                memcpy_s(pIdx, idxMemSize, desc.pIdx, idxMemSize);
                m_desc.pIdx = pIdx;
        }

        return hr;
}


void MeshBuffer::Draw(int count)
{
        UINT stride = m_desc.vtxSize;
        UINT offset = 0;

        m_vertexBuffer.Bind(0, offset);

        // 描画
        if (m_desc.idxCount > 0 && m_indexBuffer.IsValid())
        {
                m_indexBuffer.Bind();
                GetContext()->IASetPrimitiveTopology(m_desc.topology);
                GetContext()->DrawIndexed(count ? count : m_desc.idxCount, 0, 0);
        }
        else
        {
                // 頂点バッファのみで描画
                GetContext()->IASetPrimitiveTopology(m_desc.topology);
                GetContext()->Draw(count ? count : m_desc.vtxCount, 0);
        }

}

HRESULT MeshBuffer::Write(void* pVtx)
{
        if (!m_desc.isWrite) { return E_FAIL; }

        if (!m_vertexBuffer.IsValid())
        {
                DebugMessage("[MeshBuffer] Write called before vertex buffer initialization.\n");
                return E_FAIL;
        }

        HRESULT hr;
        ID3D11DeviceContext* pContext = GetContext();
        D3D11_MAPPED_SUBRESOURCE mapResource;

        // バッファ書き込み
        hr = pContext->Map(m_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapResource);
        if (SUCCEEDED(hr))
        {
                rsize_t size = m_desc.vtxCount * m_desc.vtxSize;
                memcpy_s(mapResource.pData, size, pVtx, size);
                pContext->Unmap(m_vertexBuffer.Get(), 0);
        }
        return hr;
}

MeshBuffer::Description MeshBuffer::GetDesc()
{
        return m_desc;
}

HRESULT MeshBuffer::CreateVertexBuffer(const void* pVtx, UINT size, UINT count, bool isWrite)
{
        return m_vertexBuffer.Init(pVtx, size, count, isWrite);
}

HRESULT MeshBuffer::CreateIndexBuffer(const void* pIdx, UINT size, UINT count)
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

        return m_indexBuffer.Init(pIdx, size, count);
}
