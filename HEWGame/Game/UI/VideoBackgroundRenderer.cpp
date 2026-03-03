#include "VideoBackgroundRenderer.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <sstream>
#include <chrono>

#include "DirectX.h"
#include "Diagnostics/Logger.h"

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
    constexpr uint32_t kScrollDebugLogIntervalFrames = 60;
    constexpr uint32_t kDeadRangeWarnThresholdFrames = 30;
    constexpr float kDeadRangeInputEpsilon = 1.0e-4f;
    constexpr float kDeadRangeOutputEpsilon = 1.0e-5f;
    constexpr float kClampEdgeReleaseEpsilon = 1.0e-4f;

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
        float clockTime = 0.0f;
        LONGLONG videoTime = 0;
        bool finished = false;
        bool paused = false;
        bool autoLoop = true;
        bool pendingTypeChange = false;
        GUID outputSubtype = MFVideoFormat_IYUV;

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

            hr = firstOutputType->GetGUID(MF_MT_SUBTYPE, &outputSubtype);
            if (FAILED(hr))
            {
                outputSubtype = MFVideoFormat_IYUV;
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

            hr = readerAttributes->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
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

            hr = readerOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
            if (FAILED(hr))
            {
                LogVideoError("SetGUID MF_MT_SUBTYPE NV12", hr);
                return false;
            }

            hr = sourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, readerOutputType);
            if (FAILED(hr))
            {
                LogVideoMessage("SetCurrentMediaType NV12 failed. Falling back to IYUV.");
                SAFE_RELEASE(readerOutputType);

                hr = MFCreateMediaType(&readerOutputType);
                if (FAILED(hr))
                {
                    LogVideoError("MFCreateMediaType IYUV", hr);
                    return false;
                }

                hr = readerOutputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
                if (FAILED(hr))
                {
                    LogVideoError("SetGUID MF_MT_MAJOR_TYPE IYUV", hr);
                    return false;
                }

                hr = readerOutputType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_IYUV);
                if (FAILED(hr))
                {
                    LogVideoError("SetGUID MF_MT_SUBTYPE IYUV", hr);
                    return false;
                }

                hr = sourceReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, readerOutputType);
                if (FAILED(hr))
                {
                    LogVideoError("SetCurrentMediaType IYUV", hr);
                    return false;
                }
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
        LogVideoMessage("Initialize called with null device/context.");
        return false;
    }

    m_device->QueryInterface(IID_PPV_ARGS(m_device1.ReleaseAndGetAddressOf()));
    m_nv12Supported = (m_device1 != nullptr);

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
    const auto updateStart = std::chrono::high_resolution_clock::now();

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
        LogVideoError("ReadSample", hr);
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
            LogVideoMessage("ReadOutputMediaFormat failed after media type change.");
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
                if (m_useNv12Path)
                {
                    UpdateTextureFromNV12(byteBuffer, currentLength);
                }
                else
                {
                    UpdateTextureFromIYUV(byteBuffer, currentLength);
                }
            }
            buffer->Unlock();
        }
    }

    SAFE_RELEASE(buffer);
    SAFE_RELEASE(sample);

    const auto updateEnd = std::chrono::high_resolution_clock::now();
    const float updateMs = std::chrono::duration<float, std::milli>(updateEnd - updateStart).count();
    m_updateCpuMsAccum += updateMs;
    ++m_updateCpuSampleCount;
    if ((m_updateCpuSampleCount % 120u) == 0u)
    {
        std::ostringstream oss;
        oss << kLogPrefix << "profile Update CPU avgMs=" << (m_updateCpuMsAccum / static_cast<float>(m_updateCpuSampleCount))
            << " samples=" << m_updateCpuSampleCount;
        Diagnostics::LogInfo(oss.str());
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

void VideoBackgroundRenderer::SetViewportSize(float width, float height)
{
    m_viewportWidth = width;
    m_viewportHeight = height;
}

void VideoBackgroundRenderer::SetScroll01(float scroll01)
{
    m_scroll01 = scroll01;
}

void VideoBackgroundRenderer::SetBaseOffset01(float baseOffset01)
{
    m_baseOffset01 = std::clamp(baseOffset01, 0.0f, 1.0f);
}

void VideoBackgroundRenderer::SetBaseOffsetPixels(float baseOffsetPixels)
{
    if (std::isfinite(baseOffsetPixels))
    {
        m_baseOffsetPixels = baseOffsetPixels;
    }
    else
    {
        m_baseOffsetPixels = 0.0f;
    }
}

void VideoBackgroundRenderer::SetScrollSpeed(float scrollSpeed)
{
    m_scrollSpeed = std::max(0.0f, scrollSpeed);
}

void VideoBackgroundRenderer::SetParallaxFactor(float parallaxFactor)
{
    if (!std::isfinite(parallaxFactor))
    {
        m_parallaxFactor = 1.0f;
        return;
    }

    m_parallaxFactor = std::max(0.0f, parallaxFactor);
}

void VideoBackgroundRenderer::SetScrollDirection(float dir)
{
    m_scrollDirection = (dir < 0.0f) ? -1.0f : 1.0f;
}

void VideoBackgroundRenderer::SetZoom(float zoom)
{
    m_zoom = std::max(1.0f, zoom);
}

void VideoBackgroundRenderer::SetScrollMode(ScrollMode mode)
{
    m_scrollMode = mode;
}

void VideoBackgroundRenderer::SetClampEdgeReleaseEnabled(bool enabled)
{
    m_enableClampEdgeRelease = enabled;
    m_clampEdgeReleaseArmed = false;
}

void VideoBackgroundRenderer::SetInvertY(bool invert)
{
    m_invertY = invert;
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

    DirectX::XMFLOAT4 uvTransform[1]{ DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) };

    const float viewportWidth = m_viewportWidth;
    const float viewportHeight = m_viewportHeight;
    const float sourceWidth = static_cast<float>(m_frameWidth);
    const float sourceHeight = static_cast<float>(m_frameHeight);

    const bool validViewport = std::isfinite(viewportWidth) && std::isfinite(viewportHeight)
        && viewportWidth > 0.0f && viewportHeight > 0.0f;
    const bool validSource = sourceWidth > 0.0f && sourceHeight > 0.0f;

    if (validViewport && validSource)
    {
        const float coverScale = std::max(viewportWidth / sourceWidth, viewportHeight / sourceHeight);
        if (std::isfinite(coverScale) && coverScale > 0.0f)
        {
            const float visibleSourceWidth = viewportWidth / coverScale;
            const float visibleSourceHeight = viewportHeight / coverScale;

            float uvW = visibleSourceWidth / sourceWidth;
            float uvH = visibleSourceHeight / sourceHeight;

            const float zoom = std::max(1.0f, m_zoom);
            uvW /= zoom;
            uvH /= zoom;

            uvH = std::clamp(uvH, 0.0f, 1.0f);
            uvW = std::clamp(uvW, 0.0f, 1.0f);

            const float maxY = std::max(0.0f, 1.0f - uvH);
            const float maxX = std::max(0.0f, 1.0f - uvW);

            const float scrollPx = m_worldScrollOffset * m_pixelsPerUnit * m_parallaxFactor;
            const float scrollDirection = m_scrollDirection;
            const float speedAdjustedScrollPx = scrollPx * m_scrollSpeed * scrollDirection;

            const float baseOffset01 = std::clamp(m_baseOffset01, 0.0f, 1.0f);
            const float maxOffsetPx = maxY * sourceHeight;
            const float normalizedBaseOffsetPx = baseOffset01 * maxOffsetPx;
            const float offsetAppliedScrollPx = normalizedBaseOffsetPx + m_baseOffsetPixels + speedAdjustedScrollPx;

            float normalizedY = 0.0f;
            if (maxOffsetPx > 0.0f)
            {
                if (m_scrollMode == ScrollMode::Wrap)
                {
                    const float wrappedOffsetPx = std::fmod(offsetAppliedScrollPx, maxOffsetPx);
                    const float normalizedOffsetPx = wrappedOffsetPx < 0.0f ? wrappedOffsetPx + maxOffsetPx : wrappedOffsetPx;
                    normalizedY = normalizedOffsetPx / maxOffsetPx;
                }
                else
                {
                    const float clampedOffsetPx = std::clamp(offsetAppliedScrollPx, 0.0f, maxOffsetPx);
                    normalizedY = clampedOffsetPx / maxOffsetPx;
                }
            }

            const bool hasDeadRangeHistory = m_deadRangeStateInitialized;
            const bool inputChanged = hasDeadRangeHistory
                && std::fabs(offsetAppliedScrollPx - m_lastDeadRangeInputScroll01) > kDeadRangeInputEpsilon;
            const bool outputChanged = hasDeadRangeHistory
                && std::fabs(normalizedY - m_lastDeadRangeNormalizedY) > kDeadRangeOutputEpsilon;
            if (!m_clampEdgeReleaseArmed && std::fabs(m_worldScrollOffset) > kDeadRangeInputEpsilon)
            {
                m_clampEdgeReleaseArmed = true;
            }
            const bool clampEdgeReleaseActive = m_enableClampEdgeRelease && m_clampEdgeReleaseArmed;
            const bool clampOutputStuck = (m_scrollMode == ScrollMode::Clamp) && clampEdgeReleaseActive && inputChanged && !outputChanged;

            if (clampOutputStuck)
            {
                // Guard against immediate edge sticking in clamp mode:
                // keep the UV in a tiny safe band away from exact 0/1 when input is still changing.
                if (normalizedY <= 0.0f)
                {
                    normalizedY = kClampEdgeReleaseEpsilon;
                }
                else if (normalizedY >= 1.0f)
                {
                    normalizedY = 1.0f - kClampEdgeReleaseEpsilon;
                }

                std::ostringstream clampWarn;
                clampWarn << kLogPrefix
                    << "clamp dead-range detected"
                    << " inputOffsetPx=" << offsetAppliedScrollPx
                    << " normalizedY=" << normalizedY
                    << " speed=" << m_scrollSpeed
                    << " direction=" << m_scrollDirection
                    << " baseOffset01=" << baseOffset01
                    << " clampEdgeReleaseArmed=" << (m_clampEdgeReleaseArmed ? "true" : "false");
                Diagnostics::LogInfo(clampWarn.str());
            }

            if (!hasDeadRangeHistory)
            {
                m_lastDeadRangeInputScroll01 = offsetAppliedScrollPx;
                m_lastDeadRangeNormalizedY = normalizedY;
                m_deadRangeFrameStreak = 0;
                m_deadRangeStateInitialized = true;
            }
            else
            {
                if (inputChanged && !outputChanged)
                {
                    ++m_deadRangeFrameStreak;
                    if (m_deadRangeFrameStreak >= kDeadRangeWarnThresholdFrames)
                    {
                        std::ostringstream warn;
                        warn << kLogPrefix
                            << "warn dead-range: normalizedY is not changing while input scroll changes"
                            << " inputOffsetPx=" << offsetAppliedScrollPx
                            << " normalizedY=" << normalizedY
                            << " speed=" << m_scrollSpeed
                            << " baseOffset01=" << baseOffset01
                            << " mode=" << (m_scrollMode == ScrollMode::Wrap ? "wrap" : "clamp");
                        Diagnostics::LogInfo(warn.str());
                        m_deadRangeFrameStreak = 0;
                    }
                }
                else
                {
                    m_deadRangeFrameStreak = 0;
                }

                m_lastDeadRangeInputScroll01 = offsetAppliedScrollPx;
                m_lastDeadRangeNormalizedY = normalizedY;
            }

            float uvY = normalizedY * maxY;
            if (m_invertY)
            {
                uvY = maxY - uvY;
            }

            const float uvX = 0.5f * maxX;
            uvTransform[0] = DirectX::XMFLOAT4(uvX, uvY, uvW, uvH);

            ++m_debugScrollLogFrameCounter;
            if ((m_debugScrollLogFrameCounter % kScrollDebugLogIntervalFrames) == 0)
            {
                std::ostringstream oss;
                oss << kLogPrefix
                    << "scroll cameraY=" << m_cameraY
                    << " worldScrollOffset=" << m_worldScrollOffset
                    << " scrollPx=" << scrollPx
                    << " uvY=" << uvY
                    << " uvH=" << uvH;
                Diagnostics::LogInfo(oss.str());
            }
        }
    }

    SetDepthTest(false);

    m_vertexShader->WriteBuffer(0, m_matrices);
    m_vertexShader->WriteBuffer(1, uvTransform);
    m_vertexShader->Bind();
    m_pixelShader->SetTexture(0, m_srv.Get());
    m_pixelShader->SetTexture(1, m_srvChroma.Get());
    m_pixelShader->Bind();

    SetSamplerState(SAMPLER_LINEAR);
    m_quad->Draw();

    SetDepthTest(true);
}

void VideoBackgroundRenderer::Draw(const BackgroundLayerDrawContext& context)
{
    m_cameraY = context.cameraY;
    m_worldScrollOffset = context.worldScrollOffset;
    m_pixelsPerUnit = std::max(0.0f, context.pixelsPerUnit);
    SetViewportSize(context.viewportWidth, context.viewportHeight);
    Draw();
}

void VideoBackgroundRenderer::Shutdown()
{
    m_ready = false;
    m_quad.reset();
    m_vertexShader.reset();
    m_pixelShader.reset();
    m_srv.Reset();
    m_srvChroma.Reset();
    m_texture.Reset();
    m_device1.Reset();
    m_decoder.reset();

    if (m_mfStarted)
    {
        MFShutdown();
        m_mfStarted = false;
    }

    m_useNv12Path = false;
    m_nv12Supported = false;
    m_updateCpuMsAccum = 0.0f;
    m_updateCpuSampleCount = 0;

    m_device = nullptr;
    m_context = nullptr;
    m_frameWidth = 0;
    m_frameHeight = 0;
    m_viewportWidth = 0.0f;
    m_viewportHeight = 0.0f;
    m_scroll01 = 0.0f;
    m_baseOffset01 = 0.0f;
    m_baseOffsetPixels = 0.0f;
    m_scrollSpeed = 1.0f;
    m_parallaxFactor = 1.0f;
    m_scrollDirection = 1.0f;
    m_zoom = 1.0f;
    m_cameraY = 0.0f;
    m_worldScrollOffset = 0.0f;
    m_pixelsPerUnit = 1.0f;
    m_debugScrollLogFrameCounter = 0;
    m_lastDeadRangeInputScroll01 = 0.0f;
    m_lastDeadRangeNormalizedY = 0.0f;
    m_deadRangeFrameStreak = 0;
    m_deadRangeStateInitialized = false;
    m_enableClampEdgeRelease = true;
    m_clampEdgeReleaseArmed = false;
    m_invertY = false;
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

    m_texture.Reset();
    m_srv.Reset();
    m_srvChroma.Reset();

    const bool requestNv12 = m_nv12Supported && m_decoder && (m_decoder->mf.outputSubtype == MFVideoFormat_NV12);
    m_useNv12Path = false;

    if (requestNv12)
    {
        D3D11_TEXTURE2D_DESC nv12Desc{};
        nv12Desc.Width = width;
        nv12Desc.Height = height;
        nv12Desc.MipLevels = 1;
        nv12Desc.ArraySize = 1;
        nv12Desc.Format = DXGI_FORMAT_NV12;
        nv12Desc.SampleDesc.Count = 1;
        nv12Desc.Usage = D3D11_USAGE_DEFAULT;
        nv12Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = m_device->CreateTexture2D(&nv12Desc, nullptr, m_texture.GetAddressOf());
        if (SUCCEEDED(hr) && m_device1)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC1 lumaDesc{};
            lumaDesc.Format = DXGI_FORMAT_R8_UNORM;
            lumaDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            lumaDesc.Texture2D.MostDetailedMip = 0;
            lumaDesc.Texture2D.MipLevels = 1;

            D3D11_SHADER_RESOURCE_VIEW_DESC1 chromaDesc{};
            chromaDesc.Format = DXGI_FORMAT_R8G8_UNORM;
            chromaDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            chromaDesc.Texture2D.MostDetailedMip = 0;
            chromaDesc.Texture2D.MipLevels = 1;

            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView1> lumaSrv1;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView1> chromaSrv1;

            hr = m_device1->CreateShaderResourceView1(m_texture.Get(), &lumaDesc, lumaSrv1.GetAddressOf());
            if (SUCCEEDED(hr))
            {
                hr = m_device1->CreateShaderResourceView1(m_texture.Get(), &chromaDesc, chromaSrv1.GetAddressOf());
            }

            if (SUCCEEDED(hr))
            {
                lumaSrv1.As(&m_srv);
                chromaSrv1.As(&m_srvChroma);
                m_useNv12Path = true;
                LogVideoMessage("Using NV12 GPU texture path.");
                return true;
            }
        }

        m_texture.Reset();
        m_srv.Reset();
        m_srvChroma.Reset();
        LogVideoMessage("NV12 GPU path unavailable. Falling back to IYUV upload path.");
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

bool VideoBackgroundRenderer::UpdateTextureFromNV12(const uint8_t* data, size_t dataSize)
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

    const uint32_t yPitch = m_frameWidth;
    const uint32_t uvPitch = m_frameWidth;
    const uint32_t ySize = yPitch * m_frameHeight;

    D3D11_BOX yBox{};
    yBox.left = 0;
    yBox.top = 0;
    yBox.front = 0;
    yBox.right = m_frameWidth;
    yBox.bottom = m_frameHeight;
    yBox.back = 1;

    m_context->UpdateSubresource(m_texture.Get(), 0, &yBox, data, yPitch, 0);

    D3D11_BOX uvBox{};
    uvBox.left = 0;
    uvBox.top = m_frameHeight;
    uvBox.front = 0;
    uvBox.right = m_frameWidth;
    uvBox.bottom = m_frameHeight + (m_frameHeight / 2);
    uvBox.back = 1;

    m_context->UpdateSubresource(m_texture.Get(), 0, &uvBox, data + ySize, uvPitch, 0);
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
