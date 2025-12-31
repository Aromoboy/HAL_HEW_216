// 描画で使う頂点バッファを扱う
// モデル描画がこのバッファを参照する

#pragma once

#include <wrl/client.h>
#include <d3d11.h>

namespace Core
{
        class VertexBuffer
        {
        public:
                VertexBuffer();
                ~VertexBuffer();

                HRESULT Init(const void* data, UINT stride, UINT count, bool dynamic = false);
                void Bind(UINT slot = 0, UINT offset = 0) const;
                void Shutdown();

                ID3D11Buffer* Get() const;
                UINT GetStride() const;
                UINT GetCount() const;
                bool IsDynamic() const;
                bool IsValid() const;

        private:
                Microsoft::WRL::ComPtr<ID3D11Buffer> m_buffer;
                UINT m_stride;
                UINT m_count;
                bool m_dynamic;
        };
}

