#include "VideoBackgroundRenderer.h"

#include <array>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>

#include "DirectX.h"
#include "Diagnostics/Logger.h"

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
    constexpr const char* kLogPrefix = "[VideoBackgroundRenderer] ";

    void LogVideoError(const char* step, HRESULT hr)
    {
        std::ostringstream oss;
        oss << kLogPrefix << step << " failed (hr=0x" << std::hex << static_cast<unsigned long>(hr) << ").";
        Diagnostics::LogInfo(oss.str());
    }

    void LogVideoMessage(const char* message)
    {
        std::ostringstream oss;
        oss << kLogPrefix << message;
        Diagnostics::LogInfo(oss.str());
    }

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
        double clockTime = 0.0;
        LONGLONG duration = 0;
        LONGLONG lastSampleTime = 0;
        LONGLONG lastSampleEnd = 0;
        LONGLONG nextSampleTime = 0;
        Microsoft::WRL::ComPtr<IMFSample> nextSample;
        uint32_t nextWidth = 0;
        uint32_t nextHeight = 0;
        uint32_t sampleWidth = 0;
        uint32_t sampleHeight = 0;
        bool finished = false;
        bool paused = false;
        bool autoLoop = true;
        bool endOfStream = false;
        bool catchingUp = false;

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
            IMFMediaType* nativeType = nullptr;
            HRESULT hr = sourceReader->GetNativeMediaType(
                MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                MF_SOURCE_READER_CURRENT_TYPE_INDEX,
                &nativeType);
            if (FAILED(hr))
            {
                LogVideoError("GetNativeMediaType", hr);
                SAFE_RELEASE(nativeType);
                return false;
            }

            SAFE_RELEASE(firstOutputType);
            hr = sourceReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &firstOutputType);
            if (FAILED(hr))
            {
                LogVideoError("GetCurrentMediaType", hr);
                SAFE_RELEASE(nativeType);
                return false;
            }

            hr = MFGetAttributeSize(firstOutputType, MF_MT_FRAME_SIZE, &width, &height);
            if (FAILED(hr))
            {
                LogVideoError("MFGetAttributeSize", hr);
                SAFE_RELEASE(nativeType);
                return false;
            }

            uint64_t frameRate = 0;
            hr = nativeType->GetUINT64(MF_MT_FRAME_RATE, &frameRate);
            if (SUCCEEDED(hr))
            {
                const auto denominator = LO32(frameRate);
                fps = denominator ? static_cast<float>(HI32(frameRate)) / static_cast<float>(denominator) : 0.0f;
            }

            SAFE_RELEASE(nativeType);
            return true;
        }

        bool Open(const std::string& filename)
        {
            finished = false;

            wchar_t wfilename[256]{};
            mbstowcs(wfilename, filename.c_str(), sizeof(wfilename) / sizeof(wchar_t));

            HRESULT hr = MFCreateSourceResolver(&sourceResolver);
            if (FAILED(hr))
            {
                LogVideoError("MFCreateSourceResolver", hr);
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
                LogVideoError("CreateObjectFromURL", hr);
                return false;
            }

            hr = source->QueryInterface(IID_PPV_ARGS(&mediaSource));
            if (FAILED(hr))
            {
                LogVideoError("QueryInterface mediaSource", hr);
                return false;
            }

            hr = MFCreateAttributes(&readerAttributes, 2);
            if (FAILED(hr))
            {
                LogVideoError("MFCreateAttributes", hr);
                return false;
            }

            hr = readerAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
            if (FAILED(hr))
            {
                LogVideoError("SetGUID MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE", hr);
                return false;
            }

            hr = readerAttributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, 1);
            if (FAILED(hr))
            {
                LogVideoError("SetUINT32 MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING", hr);
                return false;
            }

            hr = readerAttributes->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV);
            if (FAILED(hr))
            {
                LogVideoError("SetGUID MF_MT_SUBTYPE", hr);
                return false;
            }

            hr = MFCreateSourceReaderFromMediaSource(mediaSource, readerAttributes, &sourceReader);
            if (FAILED(hr))
            {
                LogVideoError("MFCreateSourceReaderFromMediaSource", hr);
                return false;
            }

            hr = MFCreateMediaType(&readerOutputType);
            if (FAILED(hr))
            {
                LogVideoError("MFCreateMediaType", hr);
                return false;
            }

            hr = readerOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
            if (FAILED(hr))
            {
                LogVideoError("SetGUID MF_MT_MAJOR_TYPE", hr);
                return false;
            }

            hr = readerOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV);
            if (FAILED(hr))
            {
                LogVideoError("SetGUID MF_MT_SUBTYPE", hr);
                return false;
            }

            hr = sourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, readerOutputType);
            if (FAILED(hr))
            {
                LogVideoError("SetCurrentMediaType", hr);
                return false;
            }

            if (!ReadOutputMediaFormat())
            {
                LogVideoMessage("ReadOutputMediaFormat failed.");
                return false;
            }

            hr = sourceReader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
            if (FAILED(hr))
            {
                LogVideoError("SetStreamSelection disable all", hr);
                return false;
            }

            hr = sourceReader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);
            if (FAILED(hr))
            {
                LogVideoError("SetStreamSelection video", hr);
                return false;
            }

            PROPVARIANT presentationDuration{};
            if (SUCCEEDED(sourceReader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &presentationDuration))
                && presentationDuration.vt == VT_UI8
                && presentationDuration.uhVal.QuadPart <= static_cast<ULONGLONG>((std::numeric_limits<LONGLONG>::max)()))
            {
                duration = static_cast<LONGLONG>(presentationDuration.uhVal.QuadPart);
            }
            PropVariantClear(&presentationDuration);

            clockTime = 0.0;
            return true;
        }

        bool Seek(LONGLONG time)
        {
            PROPVARIANT position{};
            position.vt = VT_I8;
            position.hVal.QuadPart = time;
            const HRESULT hr = sourceReader->SetCurrentPosition(GUID_NULL, position);
            nextSample.Reset();
            if (FAILED(hr))
            {
                LogVideoError("SetCurrentPosition", hr);
                finished = true;
                return false;
            }
            endOfStream = false;
            catchingUp = true;
            lastSampleTime = time;
            lastSampleEnd = 0;
            return true;
        }

        Microsoft::WRL::ComPtr<IMFSample> ReadDueSample(float dt)
        {
            Microsoft::WRL::ComPtr<IMFSample> selected;
            if (paused || finished || !std::isfinite(dt) || dt < 0.0f)
            {
                return selected;
            }
            clockTime += static_cast<double>(dt);
            const double durationSeconds = static_cast<double>(duration) / kMfTimeBase;
            if (autoLoop && duration > 0 && clockTime >= durationSeconds)
            {
                // ミカ：周回後も余った時間を維持して低FPSで再生速度が変わらないようにする
                clockTime = std::fmod(clockTime, durationSeconds);
                if (!Seek(static_cast<LONGLONG>(clockTime * kMfTimeBase)))
                {
                    return selected;
                }
            }

            const double maximumSeconds = static_cast<double>((std::numeric_limits<LONGLONG>::max)() / kMfTimeBase);
            const double targetSeconds = (std::min)(clockTime, duration > 0 ? durationSeconds : maximumSeconds);
            const LONGLONG target = duration > 0 && clockTime >= durationSeconds
                ? duration : static_cast<LONGLONG>(targetSeconds * kMfTimeBase);
            constexpr LONGLONG kSeekThreshold = kMfTimeBase / 2;
            if (duration > 0 && !endOfStream && !catchingUp && target - lastSampleTime > kSeekThreshold && !Seek(target))
            {
                return selected;
            }

            // ミカ：古いキーフレームから追いつく間は再シークせず一更新のデコード量を制限する
            constexpr int kMaximumReads = 8;
            int reads = 0;
            for (;;)
            {
                if (nextSample)
                {
                    if (nextSampleTime > target)
                    {
                        catchingUp = false;
                        break;
                    }
                    selected = std::move(nextSample);
                    sampleWidth = nextWidth;
                    sampleHeight = nextHeight;
                }
                if (endOfStream)
                {
                    if (!autoLoop && target >= duration)
                    {
                        finished = true;
                    }
                    break;
                }
                if (reads++ >= kMaximumReads)
                {
                    break;
                }

                DWORD streamIndex = 0;
                DWORD flags = 0;
                LONGLONG timeStamp = 0;
                Microsoft::WRL::ComPtr<IMFSample> sample;
                HRESULT hr = sourceReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,
                    &streamIndex, &flags, &timeStamp, sample.GetAddressOf());
                if (FAILED(hr))
                {
                    LogVideoError("ReadSample", hr);
                    finished = true;
                    break;
                }
                if ((flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) && !ReadOutputMediaFormat())
                {
                    finished = true;
                    break;
                }
                if (sample)
                {
                    LONGLONG sampleDuration = 0;
                    if (FAILED(sample->GetSampleDuration(&sampleDuration)) || sampleDuration <= 0)
                    {
                        sampleDuration = static_cast<LONGLONG>(kMfTimeBase / (fps > 0.0f ? fps : 30.0f));
                    }
                    lastSampleTime = timeStamp;
                    lastSampleEnd = (std::max)(lastSampleEnd, timeStamp + sampleDuration);
                    nextSampleTime = timeStamp;
                    nextWidth = width;
                    nextHeight = height;
                    nextSample = std::move(sample);
                }
                if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
                {
                    endOfStream = true;
                    if (duration <= 0)
                    {
                        duration = (std::max)(lastSampleEnd, LONGLONG{1});
                    }
                }
            }
            return selected;
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
        LogVideoMessage("Initialize called with null device/context.");
        return false;
    }

    HRESULT hr = MFStartup(MF_VERSION);
    if (FAILED(hr))
    {
        LogVideoError("MFStartup", hr);
        return false;
    }
    m_mfStarted = true;

    m_decoder = std::make_unique<DecoderState>();
    if (!m_decoder->mf.Open(mp4Path))
    {
        LogVideoMessage("Decoder open failed.");
        Shutdown();
        return false;
    }

    if (!LoadShaders(shader))
    {
        LogVideoMessage("LoadShaders failed.");
        Shutdown();
        return false;
    }

    if (!CreateFullScreenQuad())
    {
        LogVideoMessage("CreateFullScreenQuad failed.");
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
    auto sample = mf.ReadDueSample(dt);
    if (!sample)
    {
        return;
    }

    Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = sample->ConvertToContiguousBuffer(buffer.GetAddressOf());

    if (SUCCEEDED(hr))
    {
        uint8_t* byteBuffer = nullptr;
        DWORD maxLength = 0;
        DWORD currentLength = 0;
        hr = buffer->Lock(&byteBuffer, &maxLength, &currentLength);
        if (SUCCEEDED(hr))
        {
            bool textureReady = m_texture && m_srv && m_frameWidth == mf.sampleWidth && m_frameHeight == mf.sampleHeight;
            if (!textureReady)
            {
                textureReady = CreateVideoTexture(mf.sampleWidth, mf.sampleHeight);
            }

            if (textureReady && !UpdateTextureFromIYUV(byteBuffer, currentLength))
            {
                LogVideoMessage("Video frame upload failed.");
            }
            buffer->Unlock();
        }
    }

    if (FAILED(hr))
    {
        LogVideoError("Read video frame buffer", hr);
    }
}

bool VideoBackgroundRenderer::IsFinished() const
{
    return m_decoder ? m_decoder->mf.finished : true;
}

void VideoBackgroundRenderer::SetAutoLoop(bool enabled)
{
    if (m_decoder)
    {
        m_decoder->mf.autoLoop = enabled;
    }
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

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf());
    if (FAILED(hr))
    {
        LogVideoError("CreateTexture2D", hr);
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    hr = m_device->CreateShaderResourceView(texture.Get(), &srvDesc, srv.GetAddressOf());
    if (FAILED(hr))
    {
        LogVideoError("CreateShaderResourceView", hr);
        return false;
    }
    m_texture = std::move(texture);
    m_srv = std::move(srv);
    m_frameWidth = width;
    m_frameHeight = height;
    return true;
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
