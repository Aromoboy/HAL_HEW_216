#pragma once

#include <wrl/client.h>
#include <d3d11.h>

namespace Core
{
        class IndexBuffer
        {
        public:
                IndexBuffer();
                ~IndexBuffer();

                HRESULT Init(const void* data, UINT elementSize, UINT count);
                void Bind(UINT offset = 0) const;
                void Shutdown();

                ID3D11Buffer* Get() const;
                DXGI_FORMAT GetFormat() const;
                UINT GetCount() const;
                bool IsValid() const;

        private:
                Microsoft::WRL::ComPtr<ID3D11Buffer> m_buffer;
                DXGI_FORMAT m_format;
                UINT m_count;
        };
}

