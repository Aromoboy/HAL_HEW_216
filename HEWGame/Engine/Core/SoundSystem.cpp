#include "SoundSystem.h"
#include "Debug.h"
#include "Main.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <vector>

#include <mfapi.h>
#include <mferror.h>
#include <mfreadwrite.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#ifdef _XBOX //Big-Endian
#define fourccRIFF 'RIFF'
#define fourccDATA 'data'
#define fourccFMT  'fmt '
#define fourccWAVE 'WAVE'
#define fourccXWMA 'XWMA'
#define fourccDPDS 'dpds'
#endif

#ifndef _XBOX //Little-Endian
#define fourccRIFF 'FFIR'
#define fourccDATA 'atad'
#define fourccFMT  ' tmf'
#define fourccWAVE 'EVAW'
#define fourccXWMA 'AMWX'
#define fourccDPDS 'sdpd'
#endif

namespace
{
    std::string ToLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
        {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }

    std::string GetFileExtension(const std::string& filePath)
    {
        const size_t dot = filePath.find_last_of('.');
        if (dot == std::string::npos)
        {
            return {};
        }
        return ToLower(filePath.substr(dot));
    }

    std::wstring ToWideString(const std::string& value)
    {
        if (value.empty())
        {
            return {};
        }
        const int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
        if (sizeNeeded <= 0)
        {
            return {};
        }
        std::wstring wide(sizeNeeded, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wide.data(), sizeNeeded);
        if (!wide.empty() && wide.back() == L'\0')
        {
            wide.pop_back();
        }
        return wide;
    }

    void LogLoadFailure(const std::string& filePath, const std::string& reason)
    {
        std::ostringstream oss;
        oss << "[SoundSystem] Failed to load audio: " << filePath << " (" << reason << ")";
        DebugLog::DebugMessage(oss.str());
    }

    bool LoadPcmWaveFile(const std::string& filePath, WaveData* outData)
    {
        HMMIO mmioHandle = mmioOpenA(const_cast<LPSTR>(filePath.c_str()), nullptr, MMIO_READ | MMIO_ALLOCBUF);
        if (!mmioHandle)
        {
            LogLoadFailure(filePath, "mmioOpenA failed");
            return false;
        }

        MMCKINFO chunkInfo{};
        MMCKINFO riffChunkInfo{};
        riffChunkInfo.fccType = mmioFOURCC('W', 'A', 'V', 'E');

        if (mmioDescend(mmioHandle, &riffChunkInfo, nullptr, MMIO_FINDRIFF) != MMSYSERR_NOERROR)
        {
            LogLoadFailure(filePath, "RIFF chunk missing");
            mmioClose(mmioHandle, MMIO_FHOPEN);
            return false;
        }

        chunkInfo.ckid = mmioFOURCC('f', 'm', 't', ' ');
        if (mmioDescend(mmioHandle, &chunkInfo, &riffChunkInfo, MMIO_FINDCHUNK) != MMSYSERR_NOERROR)
        {
            LogLoadFailure(filePath, "fmt chunk missing");
            mmioClose(mmioHandle, MMIO_FHOPEN);
            return false;
        }

        DWORD readSize = mmioRead(mmioHandle, reinterpret_cast<HPSTR>(&outData->m_wavFormat), chunkInfo.cksize);
        if (readSize != chunkInfo.cksize)
        {
            LogLoadFailure(filePath, "fmt chunk read failed");
            mmioClose(mmioHandle, MMIO_FHOPEN);
            return false;
        }

        if (outData->m_wavFormat.wFormatTag != WAVE_FORMAT_PCM)
        {
            LogLoadFailure(filePath, "unsupported WAV format (non-PCM)");
            mmioClose(mmioHandle, MMIO_FHOPEN);
            return false;
        }

        if (mmioAscend(mmioHandle, &chunkInfo, 0) != MMSYSERR_NOERROR)
        {
            LogLoadFailure(filePath, "fmt chunk ascend failed");
            mmioClose(mmioHandle, MMIO_FHOPEN);
            return false;
        }

        chunkInfo.ckid = mmioFOURCC('d', 'a', 't', 'a');
        if (mmioDescend(mmioHandle, &chunkInfo, &riffChunkInfo, MMIO_FINDCHUNK) != MMSYSERR_NOERROR)
        {
            LogLoadFailure(filePath, "data chunk missing");
            mmioClose(mmioHandle, MMIO_FHOPEN);
            return false;
        }

        outData->m_size = chunkInfo.cksize;
        outData->m_soundBuffer = new char[chunkInfo.cksize];
        readSize = mmioRead(mmioHandle, reinterpret_cast<HPSTR>(outData->m_soundBuffer), chunkInfo.cksize);

        if (readSize != chunkInfo.cksize)
        {
            LogLoadFailure(filePath, "data chunk read failed");
            mmioClose(mmioHandle, MMIO_FHOPEN);
            delete[] outData->m_soundBuffer;
            outData->m_soundBuffer = nullptr;
            outData->m_size = 0;
            return false;
        }

        mmioClose(mmioHandle, MMIO_FHOPEN);
        return true;
    }

    bool LoadDecodedAudioWithMediaFoundation(const std::string& filePath, WaveData* outData, HRESULT& outHr)
    {
        outHr = S_OK;
        const std::wstring widePath = ToWideString(filePath);
        if (widePath.empty())
        {
            outHr = E_INVALIDARG;
            return false;
        }

        IMFSourceReader* reader = nullptr;
        outHr = MFCreateSourceReaderFromURL(widePath.c_str(), nullptr, &reader);
        if (FAILED(outHr))
        {
            return false;
        }

        IMFMediaType* mediaType = nullptr;
        outHr = MFCreateMediaType(&mediaType);
        if (FAILED(outHr))
        {
            reader->Release();
            return false;
        }

        outHr = mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        if (SUCCEEDED(outHr))
        {
            outHr = mediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
        }
        if (SUCCEEDED(outHr))
        {
            outHr = reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, mediaType);
        }
        mediaType->Release();

        if (FAILED(outHr))
        {
            reader->Release();
            return false;
        }

        IMFMediaType* outputType = nullptr;
        outHr = reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &outputType);
        if (FAILED(outHr))
        {
            reader->Release();
            return false;
        }

        WAVEFORMATEX* waveFormat = nullptr;
        UINT32 waveFormatSize = 0;
        outHr = MFCreateWaveFormatExFromMFMediaType(outputType, &waveFormat, &waveFormatSize);
        outputType->Release();
        static_cast<void>(waveFormatSize);
        if (FAILED(outHr) || !waveFormat)
        {
            reader->Release();
            return false;
        }

        std::vector<BYTE> decoded;
        bool done = false;
        while (!done)
        {
            DWORD flags = 0;
            IMFSample* sample = nullptr;
            outHr = reader->ReadSample(
                MF_SOURCE_READER_FIRST_AUDIO_STREAM,
                0,
                nullptr,
                &flags,
                nullptr,
                &sample);
            if (FAILED(outHr))
            {
                break;
            }

            if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
            {
                done = true;
            }

            if (sample)
            {
                IMFMediaBuffer* buffer = nullptr;
                outHr = sample->ConvertToContiguousBuffer(&buffer);
                if (SUCCEEDED(outHr))
                {
                    BYTE* audioData = nullptr;
                    DWORD audioSize = 0;
                    outHr = buffer->Lock(&audioData, nullptr, &audioSize);
                    if (SUCCEEDED(outHr))
                    {
                        if (audioSize > 0)
                        {
                            decoded.insert(decoded.end(), audioData, audioData + audioSize);
                        }
                        buffer->Unlock();
                    }
                    buffer->Release();
                }
                sample->Release();
                if (FAILED(outHr))
                {
                    break;
                }
            }
        }

        reader->Release();

        if (FAILED(outHr))
        {
            CoTaskMemFree(waveFormat);
            return false;
        }

        if (decoded.empty())
        {
            CoTaskMemFree(waveFormat);
            outHr = E_FAIL;
            return false;
        }

        outData->m_wavFormat = *waveFormat;
        outData->m_size = static_cast<DWORD>(decoded.size());
        outData->m_soundBuffer = new char[outData->m_size];
        std::memcpy(outData->m_soundBuffer, decoded.data(), decoded.size());
        CoTaskMemFree(waveFormat);

        return true;
    }
}

SoundSystem::SoundSystem()
    : m_pAudio2(nullptr)
    , m_pMasterVoice(nullptr)
    , m_nextHandle(1)
    , m_mediaFoundationInitialized(false)
{
#ifdef _DEBUG
    DebugLog::DebugMessage("SoundSystem initializing.");
#endif

    if (FAILED(GetComInitResult()))
    {
#ifdef _DEBUG
        DebugLog::DebugMessage("Failed to initialize COM.");
#endif
        return;
    }

    HRESULT result = S_OK;

    result = XAudio2Create(&m_pAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(result))
    {
#ifdef _DEBUG
        DebugLog::DebugMessage("Failed to create XAudio2 instance.");
#endif
        return;
    }

    result = m_pAudio2->CreateMasteringVoice(&m_pMasterVoice);
    if (FAILED(result))
    {
#ifdef _DEBUG
        DebugLog::DebugMessage("Failed to create mastering voice.");
#endif
        return;
    }

    result = MFStartup(MF_VERSION);
    if (FAILED(result))
    {
        LogLoadFailure("MediaFoundation", "MFStartup failed");
        return;
    }
    m_mediaFoundationInitialized = true;
}

SoundSystem::~SoundSystem()
{
    // 全てのSourceVoiceを停止して破棄
    for (auto& pair : m_sourceVoices)
    {
        if (pair.second)
        {
            pair.second->Stop(0);  // すぐに停止
            pair.second->FlushSourceBuffers();  // バッファをフラッシュ
            pair.second->DestroyVoice();
        }
    }
    m_sourceVoices.clear();

    // MasterVoiceを破棄
    if (m_pMasterVoice)
    {
        m_pMasterVoice->DestroyVoice();
        m_pMasterVoice = nullptr;
    }

    // XAudio2を解放
    if (m_pAudio2)
    {
        m_pAudio2->Release();
        m_pAudio2 = nullptr;
    }

    if (m_mediaFoundationInitialized)
    {
        MFShutdown();
        m_mediaFoundationInitialized = false;
    }
}


void SoundSystem::CleanupFinishedVoices()
{
    // 再生が終わったボイスを削除
    auto it = m_sourceVoices.begin();
    while (it != m_sourceVoices.end())
    {
        if (it->second)
        {
            XAUDIO2_VOICE_STATE state;
            it->second->GetState(&state);

            // キューにバッファが無い = 再生終了
            if (state.BuffersQueued == 0)
            {
                it->second->DestroyVoice();
                it = m_sourceVoices.erase(it);
                continue;
            }
        }
        ++it;
    }
}

bool SoundSystem::LoadWaveFile(const std::string& a_wFilePath, WaveData* outData)
{
    if (!outData)
    {
        LogLoadFailure(a_wFilePath, "WaveData is null");
        return false;
    }

    // 既に読み込み済みなら再読み込みしない
    if (outData->m_soundBuffer != nullptr && outData->m_size > 0)
    {
        return true;
    }

    outData->Cleanup();

    const std::string extension = GetFileExtension(a_wFilePath);
    if (extension == ".wav")
    {
        return LoadPcmWaveFile(a_wFilePath, outData);
    }

    if (extension == ".ogg" || extension == ".mp3")
    {
        if (!m_mediaFoundationInitialized)
        {
            LogLoadFailure(a_wFilePath, "Media Foundation is not initialized");
            return false;
        }

        HRESULT hr = S_OK;
        const bool loaded = LoadDecodedAudioWithMediaFoundation(a_wFilePath, outData, hr);
        if (!loaded)
        {
            std::ostringstream oss;
            oss << "Media Foundation decode failed (hr=0x" << std::hex << hr << ")";
            LogLoadFailure(a_wFilePath, oss.str());
        }
        return loaded;
    }

    LogLoadFailure(a_wFilePath, "unsupported file extension");
    return false;
}

int SoundSystem::PlayWabeSound(const std::string& fileName, WaveData* outData, bool loop, float volume)
{
    CleanupFinishedVoices();

    if (!LoadWaveFile(fileName, outData))
    {
        return -1;  // 失敗
    }

    WAVEFORMATEX waveFormat{};
    memcpy(&waveFormat, &outData->m_wavFormat, sizeof(outData->m_wavFormat));
    waveFormat.wBitsPerSample = outData->m_wavFormat.nBlockAlign * 8 / outData->m_wavFormat.nChannels;

    IXAudio2SourceVoice* pNewSourceVoice = nullptr;
    HRESULT result = m_pAudio2->CreateSourceVoice(&pNewSourceVoice, (WAVEFORMATEX*)&waveFormat);
    if (FAILED(result))
    {
        DebugMessage("SourceVoiceの作成に失敗しました");
        return -1;
    }

    pNewSourceVoice->SetVolume(volume);

    XAUDIO2_BUFFER xAudio2Buffer{};
    xAudio2Buffer.pAudioData = (BYTE*)outData->m_soundBuffer;
    xAudio2Buffer.Flags = XAUDIO2_END_OF_STREAM;
    xAudio2Buffer.AudioBytes = outData->m_size;
    xAudio2Buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

    pNewSourceVoice->SubmitSourceBuffer(&xAudio2Buffer);
    pNewSourceVoice->Start();

    // ハンドルを生成して管理
    int handle = m_nextHandle++;
    m_sourceVoices[handle] = pNewSourceVoice;

    return handle;  // ハンドルを返す
}

void SoundSystem::SetVolume(int handle, float volume)
{
    auto it = m_sourceVoices.find(handle);
    if (it != m_sourceVoices.end() && it->second)
    {
        // 0.0f ~ 1.0f にクランプ
        float clampedVolume = std::max(0.0f, std::min(1.0f, volume));
        it->second->SetVolume(clampedVolume);
    }
}

void SoundSystem::StopSound(int handle)
{
    auto it = m_sourceVoices.find(handle);
    if (it != m_sourceVoices.end() && it->second)
    {
        it->second->Stop(0);  // すぐに停止
        it->second->FlushSourceBuffers();  // バッファをフラッシュ
        it->second->DestroyVoice();
        m_sourceVoices.erase(it);
    }
}

void SoundSystem::StopAllSounds()
{
    for (auto& pair : m_sourceVoices)
    {
        if (pair.second)
        {
            pair.second->Stop(0);
            pair.second->FlushSourceBuffers();
            pair.second->DestroyVoice();
        }
    }
    m_sourceVoices.clear();
}
