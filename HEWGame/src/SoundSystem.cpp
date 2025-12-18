#include "SoundSystem.h"
#include "Debug.h"

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

SoundSystem::SoundSystem()
    : m_pAudio2(nullptr)
    , m_pMasterVoice(nullptr)
    , m_nextHandle(1)
{
#ifdef _DEBUG
    DebugLog::DebugMessage("SoundSystem initializing.");
#endif

    HRESULT result = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(result))
    {
#ifdef _DEBUG
        DebugLog::DebugMessage("Failed to initialize COM.");
#endif
        return;
    }

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

    CoUninitialize();
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
        return false;
    }

    // 既に読み込み済みなら再読み込みしない
    if (outData->m_soundBuffer != nullptr && outData->m_size > 0)
    {
        return true;
    }

    delete[] outData->m_soundBuffer;
    outData->m_soundBuffer = nullptr;

    HMMIO mmioHandle = mmioOpenA(const_cast<LPSTR>(a_wFilePath.c_str()), nullptr, MMIO_READ | MMIO_ALLOCBUF);
    if (!mmioHandle)
    {
#ifdef _DEBUG
        DebugLog::DebugMessage("Failed to open wave file.");
#endif
        return false;
    }

    MMCKINFO chunkInfo{};
    MMCKINFO riffChunkInfo{};
    riffChunkInfo.fccType = mmioFOURCC('W', 'A', 'V', 'E');

    if (mmioDescend(mmioHandle, &riffChunkInfo, nullptr, MMIO_FINDRIFF) != MMSYSERR_NOERROR)
    {
#ifdef _DEBUG
        DebugMessage("Sound.cpp:Riffチャンクに侵入失敗しました");
#endif
        mmioClose(mmioHandle, MMIO_FHOPEN);
        return false;
    }

    chunkInfo.ckid = mmioFOURCC('f', 'm', 't', ' ');
    if (mmioDescend(mmioHandle, &chunkInfo, &riffChunkInfo, MMIO_FINDCHUNK) != MMSYSERR_NOERROR)
    {
#ifdef _DEBUG
        DebugMessage("Sound.cpp:fmtチャンクが存在しません");
#endif
        mmioClose(mmioHandle, MMIO_FHOPEN);
        return false;
    }

    DWORD readSize = mmioRead(mmioHandle, (HPSTR)&outData->m_wavFormat, chunkInfo.cksize);
    if (readSize != chunkInfo.cksize)
    {
#ifdef _DEBUG
        DebugMessage("Sound.h:読み込みサイズが一致していません");
#endif
        mmioClose(mmioHandle, MMIO_FHOPEN);
        return false;
    }

    if (outData->m_wavFormat.wFormatTag != WAVE_FORMAT_PCM)
    {
        mmioClose(mmioHandle, MMIO_FHOPEN);
        return false;
    }

    if (mmioAscend(mmioHandle, &chunkInfo, 0) != MMSYSERR_NOERROR)
    {
#ifdef _DEBUG
        DebugMessage("Sound.h:fmtチャンクからの退出に失敗しました");
#endif
        mmioClose(mmioHandle, MMIO_FHOPEN);
        return false;
    }

    chunkInfo.ckid = mmioFOURCC('d', 'a', 't', 'a');
    if (mmioDescend(mmioHandle, &chunkInfo, &riffChunkInfo, MMIO_FINDCHUNK) != MMSYSERR_NOERROR)
    {
#ifdef _DEBUG
        DebugMessage("Sound.cpp:dataチャンクへの侵入に失敗しました");
#endif
        mmioClose(mmioHandle, MMIO_FHOPEN);
        return false;
    }

    outData->m_size = chunkInfo.cksize;
    outData->m_soundBuffer = new char[chunkInfo.cksize];
    readSize = mmioRead(mmioHandle, (HPSTR)outData->m_soundBuffer, chunkInfo.cksize);

    if (readSize != chunkInfo.cksize)
    {
#ifdef _DEBUG
        DebugMessage("Sound.cpp:dataチャンクの読み込みに失敗しました");
#endif
        mmioClose(mmioHandle, MMIO_FHOPEN);
        delete[] outData->m_soundBuffer;
        return false;
    }

    mmioClose(mmioHandle, MMIO_FHOPEN);
    return true;
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

