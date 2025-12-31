// 描画で使う索引バッファを扱う
// モデル描画がこのバッファを参照する

#include "Core/IndexBuffer.h"

#include "Debug.h"
#include "DirectX.h"

namespace Core
{
        IndexBuffer::IndexBuffer()
                : m_format(DXGI_FORMAT_UNKNOWN)
                , m_count(0)
        {
        }

        IndexBuffer::~IndexBuffer()
        {
                Shutdown();
        }

        HRESULT IndexBuffer::Init(const void* data, UINT elementSize, UINT count)
        {
                if (elementSize != 2 && elementSize != 4)
                {
                        DebugLog::DebugMessage("[IndexBuffer] Unsupported index element size.\n");
                        return E_FAIL;
                }

                m_format = (elementSize == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
                m_count = count;

                D3D11_BUFFER_DESC desc = {};
                desc.ByteWidth = elementSize * count;
                desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
                desc.Usage = D3D11_USAGE_DEFAULT;

                D3D11_SUBRESOURCE_DATA subResource = {};
                subResource.pSysMem = data;

                HRESULT hr = GetDevice()->CreateBuffer(&desc, data ? &subResource : nullptr, m_buffer.GetAddressOf());
                if (FAILED(hr))
                {
                        DebugLog::DebugMessage("[IndexBuffer] Failed to create buffer.\n");
                        Shutdown();
                        return hr;
                }

                return hr;
        }

        void IndexBuffer::Bind(UINT offset) const
        {
                ID3D11Buffer* buffer = m_buffer.Get();
                if (!buffer)
                {
                        DebugLog::DebugMessage("[IndexBuffer] Bind called with null buffer.\n");
                        return;
                }

                GetContext()->IASetIndexBuffer(buffer, m_format, offset);
        }

        void IndexBuffer::Shutdown()
        {
                m_buffer.Reset();
                m_format = DXGI_FORMAT_UNKNOWN;
                m_count = 0;
        }

        ID3D11Buffer* IndexBuffer::Get() const
        {
                return m_buffer.Get();
        }

        DXGI_FORMAT IndexBuffer::GetFormat() const
        {
                return m_format;
        }

        UINT IndexBuffer::GetCount() const
        {
                return m_count;
        }

        bool IndexBuffer::IsValid() const
        {
                return m_buffer != nullptr;
        }
}

