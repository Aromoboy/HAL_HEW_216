#include "VideoBackgroundRenderer.h"

#include <array>
#include <cassert>
#include <cstring>

#include "DirectX.h"

// MF API usage copied and adapted from mediafoundationsamples\MFMP4ToYUVWithoutMFT
#include <mfapi.h>
#include <mferror.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <wmcodecdsp.h>

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfplay.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid.lib")

namespace
{
    constexpr LONGLONG kMfTimeBase = 10000000;

    struct MfState
    {
        IMFSourceResolver* sourceResolver = nullptr;
        IUnknown* source = nullptr;
        IMFMediaSource* mediaSource = nullptr;
        IMFAttributes* readerAttributes = nullptr;
        IMFSourceReader* sourceReader = nullptr;
        IMFMediaType* readerOutputType = nullptr;
        IMFMediaType* firstOutputType = nullptr;
        MF_OBJECT_TYPE objectType = MF_OBJECT_INVALID;

        uint32_t width = 0;
        uint32_t height = 0;
        float fps = 0.0f;
        float clockTime = 0.0f;
        LONGLONG videoTime = 0;
        bool finished = false;
        bool paused = false;
        bool autoLoop = true;
        bool pendingTypeChange = false;

        ~MfState()
        {
            SAFE_RELEASE(sourceResolver);
            SAFE_RELEASE(source);
            SAFE_RELEASE(mediaSource);
            SAFE_RELEASE(readerAttributes);
            SAFE_RELEASE(sourceReader);
            SAFE_RELEASE(readerOutputType);
            SAFE_RELEASE(firstOutputType);
        }

        bool ReadOutputMediaFormat()
        {
            assert(sourceReader);
            HRESULT hr = S_OK;
            IMFMediaType* nativeType = nullptr;
            if (FAILED(sourceReader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                    MF_SOURCE_READER_CURRENT_TYPE_INDEX,
                    &nativeType)))
            {
                SAFE_RELEASE(nativeType);
                return false;
            }

            SAFE_RELEASE(firstOutputType);
            if (FAILED(sourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &firstOutputType)))
            {
                SAFE_RELEASE(nativeType);
                return false;
            }

            if (FAILED(MFGetAttributeSize(firstOutputType, MF_MT_FRAME_SIZE, &width, &height)))
            {
                SAFE_RELEASE(nativeType);
                return false;
            }

            uint64_t frameRate = 0;
            hr = nativeType->GetUINT64(MF_MT_FRAME_RATE, &frameRate);
            if (SUCCEEDED(hr))
            {
                fps = static_cast<float>(HI32(frameRate)) / static_cast<float>(LO32(frameRate));
            }

            SAFE_RELEASE(nativeType);
            return true;
        }

        bool Open(const std::string& filename)
        {
            finished = false;
            pendingTypeChange = false;

            wchar_t wfilename[256]{};
            mbstowcs(wfilename, filename.c_str(), sizeof(wfilename) / sizeof(wchar_t));

            HRESULT hr = MFCreateSourceResolver(&sourceResolver);
            if (FAILED(hr))
            {
                return false;
            }

            hr = sourceResolver->CreateObjectFromURL(
                wfilename,
                MF_RESOLUTION_MEDIASOURCE,
                nullptr,
                &objectType,
                &source);
            if (FAILED(hr))
            {
                return false;
            }

            hr = source->QueryInterface(IID_PPV_ARGS(&mediaSource));
            if (FAILED(hr))
            {
                return false;
            }

            hr = MFCreateAttributes(&readerAttributes, 2);
            if (FAILED(hr))
            {
                return false;
            }

            hr = readerAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
            if (FAILED(hr))
            {
                return false;
            }

            hr = readerAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, 1);
            if (FAILED(hr))
            {
                return false;
            }

            hr = readerAttributes->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV);
            if (FAILED(hr))
            {
                return false;
            }

            hr = MFCreateSourceReaderFromMediaSource(mediaSource, readerAttributes, &sourceReader);
            if (FAILED(hr))
            {
                return false;
            }

            hr = MFCreateMediaType(&readerOutputType);
            if (FAILED(hr))
            {
                return false;
            }

            hr = readerOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            if (FAILED(hr))
            {
                return false;
            }

            hr = readerOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV);
            if (FAILED(hr))
            {
                return false;
            }

            hr = sourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, readerOutputType);
            if (FAILED(hr))
            {
                return false;
            }

            if (!ReadOutputMediaFormat())
            {
                return false;
            }

            hr = sourceReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
            if (FAILED(hr))
            {
                return false;
            }

            hr = sourceReader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
            if (FAILED(hr))
            {
                return false;
            }

            clockTime = 0.0f;
            videoTime = 0;
            return true;
        }
    };
}

struct VideoBackgroundRenderer::DecoderState
{
    MfState mf;
};

VideoBackgroundRenderer::VideoBackgroundRenderer()
{
    DirectX::XMStoreFloat4x4(&m_matrices[0], DirectX::XMMatrixIdentity());
    DirectX::XMStoreFloat4x4(&m_matrices[1], DirectX::XMMatrixIdentity());
    DirectX::XMStoreFloat4x4(&m_matrices[2], DirectX::XMMatrixIdentity());
}

VideoBackgroundRenderer::~VideoBackgroundRenderer()
{
    Shutdown();
}

bool VideoBackgroundRenderer::Initialize(ID3D11Device* device,
    ID3D11DeviceContext* context,
    const ShaderPaths& shader,
    const std::string& mp4Path)
{
    Shutdown();

    m_device = device;
    m_context = context;
    if (!m_device || !m_context)
    {
        return false;
    }

    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr))
    {
        return false;
    }
    m_mfStarted = true;

    m_decoder = std::make_unique<DecoderState>();
    if (!m_decoder->mf.Open(mp4Path))
    {
        Shutdown();
        return false;
    }

    if (!LoadShaders(shader))
    {
        Shutdown();
        return false;
    }

    if (!CreateFullScreenQuad())
    {
        Shutdown();
        return false;
    }

    m_ready = true;
    return true;
}

void VideoBackgroundRenderer::Update(float dt)
{
    if (!m_ready || !m_decoder)
    {
        return;
    }

    MfState& mf = m_decoder->mf;
    if (mf.paused || mf.finished)
    {
        return;
    }

    mf.clockTime += dt;
    const LONGLONG uct = static_cast<LONGLONG>(mf.clockTime * kMfTimeBase);
    if (uct < mf.videoTime)
    {
        return;
    }

    DWORD streamIndex = 0;
    DWORD flags = 0;
    LONGLONG timeStamp = 0;
    IMFSample* sample = nullptr;
    HRESULT hr = mf.sourceReader->ReadSample(
        MF_SOURCE_READER_FIRST_VIDEO_STREAM,
        0,
        &streamIndex,
        &flags,
        &timeStamp,
        &sample);

    if (FAILED(hr))
    {
        mf.finished = true;
        SAFE_RELEASE(sample);
        return;
    }

    mf.videoTime = timeStamp;

    if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
    {
        if (mf.autoLoop)
        {
            PROPVARIANT var = {};
            var.vt = VT_I8;
            hr = mf.sourceReader->SetCurrentPosition(GUID_NULL, var);
            mf.clockTime = 0.0f;
            mf.videoTime = 0;
        }
        else
        {
            mf.finished = true;
        }
    }

    if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
    {
        if (!mf.ReadOutputMediaFormat())
        {
            mf.finished = true;
        }
        else
        {
            mf.pendingTypeChange = true;
        }
    }

    if (!sample)
    {
        return;
    }

    IMFMediaBuffer* buffer = nullptr;
    DWORD bufferLength = 0;
    hr = sample->ConvertToContiguousBuffer(&buffer);
    if (SUCCEEDED(hr))
    {
        hr = buffer->GetCurrentLength(&bufferLength);
    }

    if (SUCCEEDED(hr))
    {
        uint8_t* byteBuffer = nullptr;
        DWORD maxLength = 0;
        DWORD currentLength = 0;
        hr = buffer->Lock(&byteBuffer, &maxLength, &currentLength);
        if (SUCCEEDED(hr))
        {
            if (!m_texture || mf.pendingTypeChange)
            {
                if (CreateVideoTexture(mf.width, mf.height))
                {
                    mf.pendingTypeChange = false;
                }
            }

            if (m_texture)
            {
                UpdateTextureFromIYUV(byteBuffer, currentLength);
            }
            buffer->Unlock();
        }
    }

    SAFE_RELEASE(buffer);
    SAFE_RELEASE(sample);
}

ID3D11ShaderResourceView* VideoBackgroundRenderer::GetTexture() const
{
    return m_srv.Get();
}

void VideoBackgroundRenderer::Draw()
{
    if (!m_ready || !m_quad || !m_vertexShader || !m_pixelShader || !m_srv)
    {
        return;
    }

    m_vertexShader->WriteBuffer(0, m_matrices);
    m_vertexShader->Bind();
    m_pixelShader->SetTexture(0, m_srv.Get());
    m_pixelShader->Bind();

    SetSamplerState(SAMPLER_LINEAR);
    m_quad->Draw();
}

void VideoBackgroundRenderer::Shutdown()
{
    m_ready = false;
    m_quad.reset();
    m_vertexShader.reset();
    m_pixelShader.reset();
    m_srv.Reset();
    m_texture.Reset();
    m_decoder.reset();

    if (m_mfStarted)
    {
        MFShutdown();
        m_mfStarted = false;
    }

    m_device = nullptr;
    m_context = nullptr;
    m_frameWidth = 0;
    m_frameHeight = 0;
}

bool VideoBackgroundRenderer::CreateFullScreenQuad()
{
    struct Vertex
    {
        DirectX::XMFLOAT4 pos;
        DirectX::XMFLOAT2 tex;
        DirectX::XMFLOAT4 color;
    };

    const DirectX::XMFLOAT4 white(1.0f, 1.0f, 1.0f, 1.0f);
    Vertex vertices[] = {
        {{-1.0f,  1.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, white},
        {{ 1.0f,  1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, white},
        {{-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, white},
        {{ 1.0f,  1.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, white},
        {{ 1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, white},
        {{-1.0f, -1.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, white},
    };

    MeshBuffer::Description desc{};
    desc.pVtx = vertices;
    desc.vtxSize = sizeof(Vertex);
    desc.vtxCount = static_cast<UINT>(std::size(vertices));
    desc.isWrite = false;
    desc.pIdx = nullptr;
    desc.idxSize = 0;
    desc.idxCount = 0;
    desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    m_quad = std::make_unique<MeshBuffer>();
    if (!m_quad)
    {
        return false;
    }
    return SUCCEEDED(m_quad->Create(desc));
}

bool VideoBackgroundRenderer::LoadShaders(const ShaderPaths& shader)
{
    m_vertexShader = std::make_unique<VertexShader>();
    m_pixelShader = std::make_unique<PixelShader>();
    if (!m_vertexShader || !m_pixelShader)
    {
        return false;
    }

    if (FAILED(m_vertexShader->Load(shader.vertexShader.string().c_str())))
    {
        return false;
    }

    if (FAILED(m_pixelShader->Load(shader.pixelShader.string().c_str())))
    {
        return false;
    }

    return true;
}

bool VideoBackgroundRenderer::CreateVideoTexture(uint32_t width, uint32_t height)
{
    m_frameWidth = width;
    m_frameHeight = height;

    if (!m_device || width == 0 || height == 0)
    {
        return false;
    }

    const uint32_t textureHeight = height * 2;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = textureHeight;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    m_texture.Reset();
    m_srv.Reset();

    HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, m_texture.GetAddressOf());
    if (FAILED(hr))
    {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    hr = m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srv.GetAddressOf());
    return SUCCEEDED(hr);
}

bool VideoBackgroundRenderer::UpdateTextureFromIYUV(const uint8_t* data, size_t dataSize)
{
    if (!data || !m_texture || !m_context || m_frameWidth == 0 || m_frameHeight == 0)
    {
        return false;
    }

    const size_t expectedSize = static_cast<size_t>(m_frameWidth) * m_frameHeight * 3 / 2;
    if (dataSize < expectedSize)
    {
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = m_context->Map(m_texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr))
    {
        return false;
    }

    const uint8_t* src = data;
    uint8_t* dst = static_cast<uint8_t*>(mapped.pData);
    const uint32_t bytesPerRow = m_frameWidth;
    const uint32_t yLines = m_frameHeight;

    for (uint32_t y = 0; y < yLines; ++y)
    {
        memcpy(dst, src, bytesPerRow);
        src += bytesPerRow;
        dst += mapped.RowPitch;
    }

    const uint32_t uvBytesPerRow = bytesPerRow / 2;
    const uint32_t uvLines = m_frameHeight;
    for (uint32_t y = 0; y < uvLines; ++y)
    {
        memcpy(dst, src, uvBytesPerRow);
        src += uvBytesPerRow;
        dst += mapped.RowPitch;
    }

    m_context->Unmap(m_texture.Get(), 0);
    return true;
}
