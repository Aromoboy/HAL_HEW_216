// 描画で使う頂点バッファを扱う
// モデル描画がこのバッファを参照する

#include "Core/VertexBuffer.h"

#include "Debug.h"
#include "DirectX.h"

namespace Core
{
        VertexBuffer::VertexBuffer()
                : m_stride(0)
                , m_count(0)
                , m_dynamic(false)
        {
        }

        VertexBuffer::~VertexBuffer()
        {
                Shutdown();
        }

        HRESULT VertexBuffer::Init(const void* data, UINT stride, UINT count, bool dynamic)
        {
                m_stride = stride;
                m_count = count;
                m_dynamic = dynamic;

                D3D11_BUFFER_DESC desc = {};
                desc.ByteWidth = stride * count;
                desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                desc.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
                desc.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;

                D3D11_SUBRESOURCE_DATA subResource = {};
                subResource.pSysMem = data;

                HRESULT hr = GetDevice()->CreateBuffer(&desc, data ? &subResource : nullptr, m_buffer.GetAddressOf());
                if (FAILED(hr))
                {
                        DebugLog::DebugMessage("[VertexBuffer] Failed to create buffer.\n");
                        Shutdown();
                        return hr;
                }

                return hr;
        }

        void VertexBuffer::Bind(UINT slot, UINT offset) const
        {
                ID3D11Buffer* buffer = m_buffer.Get();
                if (!buffer)
                {
                        DebugLog::DebugMessage("[VertexBuffer] Bind called with null buffer.\n");
                        return;
                }

                UINT stride = m_stride;
                GetContext()->IASetVertexBuffers(slot, 1, &buffer, &stride, &offset);
        }

        void VertexBuffer::Shutdown()
        {
                m_buffer.Reset();
                m_stride = 0;
                m_count = 0;
                m_dynamic = false;
        }

        ID3D11Buffer* VertexBuffer::Get() const
        {
                return m_buffer.Get();
        }

        UINT VertexBuffer::GetStride() const
        {
                return m_stride;
        }

        UINT VertexBuffer::GetCount() const
        {
                return m_count;
        }

        bool VertexBuffer::IsDynamic() const
        {
                return m_dynamic;
        }

        bool VertexBuffer::IsValid() const
        {
                return m_buffer != nullptr;
        }
}

