#include "MeshDX.h"

#include "Debug.h"
#include <cstddef>
#include <cstring>
#include <vector>

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
        if (desc.topology != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
        {
                m_topology = desc.topology;
        }

        if (!desc.pVtx || desc.vtxCount == 0)
        {
                DebugMessage("[MeshDX] Invalid vertex data passed to MeshDX constructor.\n");
                return;
        }

        // 頂点バッファ作成
        const UINT vertexStride = MeshDX::VertexStride;
        const std::byte* vertexBase = static_cast<const std::byte*>(desc.pVtx);
        const bool needsRebuild = desc.vtxSize != vertexStride;
        std::vector<MeshDX::Vertex> rebuiltVertices;

        const void* vertexData = desc.pVtx;
        if (needsRebuild)
        {
                DebugMessage("[MeshDX] Received unexpected vertex size. Rebuilding to canonical layout.\n");

                if (desc.vtxSize < sizeof(float) * 3)
                {
                        DebugMessage("[MeshDX] Incoming vertex size is too small to contain position data.\n");
                        return;
                }

                rebuiltVertices.resize(desc.vtxCount);
                for (UINT i = 0; i < desc.vtxCount; ++i)
                {
                        const std::byte* src = vertexBase + (static_cast<std::size_t>(i) * desc.vtxSize);

                        MeshDX::Vertex v{};
                        std::memcpy(&v.pos, src, sizeof(v.pos));

                        if (desc.vtxSize >= sizeof(float) * 5)
                        {
                                const std::byte* uvSrc = src + (desc.vtxSize - sizeof(v.uv));
                                std::memcpy(&v.uv, uvSrc, sizeof(v.uv));
                        }

                        rebuiltVertices[i] = v;
                }

                vertexData = rebuiltVertices.data();
        }

        hr = CreateVertexBuffer(vertexData, vertexStride, desc.vtxCount);
        if (FAILED(hr))
        {
#ifdef _DEBUG
                DebugMessage("頂点バッファの作成に失敗しました");
#endif
                return;
        }

        // インデックスバッファ作成
        if (desc.idxCount > 0)
        {
                const bool hasValidIndices = desc.pIdx && desc.idxSize == sizeof(unsigned int);
                const void* indexData = desc.pIdx;
                std::vector<unsigned int> generatedIndices;

                if (!hasValidIndices)
                {
                        generatedIndices.resize(desc.idxCount);
                        const UINT cycle = desc.vtxCount > 0 ? desc.vtxCount : desc.idxCount;
                        for (UINT i = 0; i < desc.idxCount; ++i)
                        {
                                generatedIndices[i] = cycle > 0 ? (i % cycle) : i;
                        }

                        DebugMessage("[MeshDX] Index buffer missing or invalid. Generated sequential indices.\n");
                        indexData = generatedIndices.data();
                }

                hr = CreateIndexBuffer(indexData, sizeof(unsigned int), desc.idxCount);
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
