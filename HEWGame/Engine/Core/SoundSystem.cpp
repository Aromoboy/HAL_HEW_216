#include "SoundSystem.h"
#include "Debug.h"
#include "Main.h"
#include <limits>
#include <new>

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

    // ミカ：失敗した読込のポインタとサイズを次回へ持ち越さない
    outData->Cleanup();
    outData->m_wavFormat = {};

    HMMIO mmioHandle = mmioOpenA(const_cast<LPSTR>(a_wFilePath.c_str()), nullptr, MMIO_READ | MMIO_ALLOCBUF);
    if (!mmioHandle)
    {
#ifdef _DEBUG
        DebugLog::DebugMessage("Failed to open wave file.");
#endif
        return false;
    }

    // ミカ：MMIOが切り詰める前にRIFFの宣言長を実ファイル長と照合する
    DWORD riffHeader[3]{};
    const LONG headerSize = static_cast<LONG>(sizeof(riffHeader));
    const LONG headerBytes = mmioRead(mmioHandle, reinterpret_cast<HPSTR>(riffHeader), headerSize);
    const LONG fileBytes = mmioSeek(mmioHandle, 0, SEEK_END);
    const LONG rewindPosition = mmioSeek(mmioHandle, 0, SEEK_SET);
    if (headerBytes != headerSize || fileBytes < headerSize || rewindPosition != 0
        || riffHeader[0] != mmioFOURCC('R', 'I', 'F', 'F') || riffHeader[2] != mmioFOURCC('W', 'A', 'V', 'E')
        || riffHeader[1] < 4 || riffHeader[1] > static_cast<DWORD>(fileBytes) - 8)
    {
        mmioClose(mmioHandle, 0);
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
        mmioClose(mmioHandle, 0);
        return false;
    }

    chunkInfo.ckid = mmioFOURCC('f', 'm', 't', ' ');
    if (mmioDescend(mmioHandle, &chunkInfo, &riffChunkInfo, MMIO_FINDCHUNK) != MMSYSERR_NOERROR)
    {
#ifdef _DEBUG
        DebugMessage("Sound.cpp:fmtチャンクが存在しません");
#endif
        mmioClose(mmioHandle, 0);
        return false;
    }

    // ミカ：PCMの形式領域を検証し、構造体の範囲を超える読込を防ぐ
    if (chunkInfo.cksize != sizeof(PCMWAVEFORMAT) && chunkInfo.cksize != sizeof(WAVEFORMATEX))
    {
        mmioClose(mmioHandle, 0);
        return false;
    }
    WAVEFORMATEX waveFormat{};
    LONG readSize = mmioRead(mmioHandle, reinterpret_cast<HPSTR>(&waveFormat),
        static_cast<LONG>(chunkInfo.cksize));
    if (readSize != static_cast<LONG>(chunkInfo.cksize))
    {
#ifdef _DEBUG
        DebugMessage("Sound.h:読み込みサイズが一致していません");
#endif
        mmioClose(mmioHandle, 0);
        return false;
    }

    const bool supportedBits = waveFormat.wBitsPerSample == 8 || waveFormat.wBitsPerSample == 16
        || waveFormat.wBitsPerSample == 24 || waveFormat.wBitsPerSample == 32;
    const DWORD blockAlign = static_cast<DWORD>(waveFormat.nChannels) * waveFormat.wBitsPerSample / 8;
    const unsigned long long averageBytes = static_cast<unsigned long long>(waveFormat.nSamplesPerSec) * blockAlign;
    if (waveFormat.wFormatTag != WAVE_FORMAT_PCM || waveFormat.cbSize != 0
        || waveFormat.nChannels == 0 || waveFormat.nChannels > XAUDIO2_MAX_AUDIO_CHANNELS
        || !supportedBits || waveFormat.nSamplesPerSec < XAUDIO2_MIN_SAMPLE_RATE
        || waveFormat.nSamplesPerSec > XAUDIO2_MAX_SAMPLE_RATE
        || waveFormat.nBlockAlign != blockAlign || averageBytes != waveFormat.nAvgBytesPerSec)
    {
        mmioClose(mmioHandle, 0);
        return false;
    }

    if (mmioAscend(mmioHandle, &chunkInfo, 0) != MMSYSERR_NOERROR)
    {
#ifdef _DEBUG
        DebugMessage("Sound.h:fmtチャンクからの退出に失敗しました");
#endif
        mmioClose(mmioHandle, 0);
        return false;
    }

    chunkInfo.ckid = mmioFOURCC('d', 'a', 't', 'a');
    if (mmioDescend(mmioHandle, &chunkInfo, &riffChunkInfo, MMIO_FINDCHUNK) != MMSYSERR_NOERROR)
    {
#ifdef _DEBUG
        DebugMessage("Sound.cpp:dataチャンクへの侵入に失敗しました");
#endif
        mmioClose(mmioHandle, 0);
        return false;
    }

    if (chunkInfo.cksize == 0 || chunkInfo.cksize > static_cast<DWORD>((std::numeric_limits<LONG>::max)())
        || chunkInfo.cksize > XAUDIO2_MAX_BUFFER_BYTES || chunkInfo.cksize % waveFormat.nBlockAlign != 0)
    {
        mmioClose(mmioHandle, 0);
        return false;
    }
    // ミカ：読込完了までバッファを局所所有し、失敗時に解放済みポインタを公開しない
    std::unique_ptr<char[]> buffer(new (std::nothrow) char[chunkInfo.cksize]);
    if (!buffer)
    {
        mmioClose(mmioHandle, 0);
        return false;
    }
    readSize = mmioRead(mmioHandle, buffer.get(), static_cast<LONG>(chunkInfo.cksize));

    if (readSize != static_cast<LONG>(chunkInfo.cksize))
    {
#ifdef _DEBUG
        DebugMessage("Sound.cpp:dataチャンクの読み込みに失敗しました");
#endif
        mmioClose(mmioHandle, 0);
        return false;
    }

    mmioClose(mmioHandle, 0);
    outData->m_wavFormat = waveFormat;
    outData->m_size = chunkInfo.cksize;
    outData->m_soundBuffer = buffer.release();
    return true;
}

int SoundSystem::PlayWabeSound(const std::string& fileName, WaveData* outData, bool loop, float volume)
{
    // ミカ：音声初期化に失敗した環境では再生要求だけを安全に拒否する
    if (!m_pAudio2 || !m_pMasterVoice)
    {
        return -1;
    }
    CleanupFinishedVoices();

    if (!LoadWaveFile(fileName, outData))
    {
        return -1;  // 失敗
    }

    WAVEFORMATEX waveFormat{};
    memcpy(&waveFormat, &outData->m_wavFormat, sizeof(outData->m_wavFormat));

    IXAudio2SourceVoice* pNewSourceVoice = nullptr;
    HRESULT result = m_pAudio2->CreateSourceVoice(&pNewSourceVoice, (WAVEFORMATEX*)&waveFormat);
    if (FAILED(result))
    {
        DebugMessage("SourceVoiceの作成に失敗しました");
        return -1;
    }

    result = pNewSourceVoice->SetVolume(volume);
    if (FAILED(result))
    {
        pNewSourceVoice->DestroyVoice();
        return -1;
    }

    XAUDIO2_BUFFER xAudio2Buffer{};
    xAudio2Buffer.pAudioData = (BYTE*)outData->m_soundBuffer;
    xAudio2Buffer.Flags = XAUDIO2_END_OF_STREAM;
    xAudio2Buffer.AudioBytes = outData->m_size;
    xAudio2Buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

    // ミカ：投入や再生開始の失敗時はボイスを登録せず破棄する
    result = pNewSourceVoice->SubmitSourceBuffer(&xAudio2Buffer);
    if (SUCCEEDED(result))
    {
        result = pNewSourceVoice->Start();
    }
    if (FAILED(result))
    {
        pNewSourceVoice->DestroyVoice();
        return -1;
    }

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
        // For looping sounds, exit loop first
        it->second->ExitLoop(XAUDIO2_COMMIT_NOW);

        // Then stop immediately
        it->second->Stop(0, XAUDIO2_COMMIT_NOW);
        it->second->FlushSourceBuffers();

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
            pair.second->ExitLoop(XAUDIO2_COMMIT_NOW);  
            pair.second->Stop(0, XAUDIO2_COMMIT_NOW);   
            pair.second->FlushSourceBuffers();
            pair.second->DestroyVoice();
        }
    }
    m_sourceVoices.clear();
}
