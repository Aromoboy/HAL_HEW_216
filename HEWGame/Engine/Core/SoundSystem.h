#ifndef ___SOUND_H___
#define ___SOUND_H___

#include <windows.h>
#pragma comment(lib, "xaudio2.lib")
#include <xaudio2.h>
#pragma comment(lib, "winmm.lib")
#include "mmsystem.h"
#include <string>
#include <memory>
#include <vector>
#include <map>

struct WaveData
{
    WAVEFORMATEX m_wavFormat;
    char* m_soundBuffer;
    DWORD m_size;

    WaveData()
    {
        m_soundBuffer = nullptr;
        m_size = 0;
        memset(&m_wavFormat, 0, sizeof(m_wavFormat));
    }

    // デストラクタでバッファを削除しない（XAudio2が使用中の可能性があるため）
    ~WaveData()
    {
        // 何もしない - メモリリークするが、クラッシュよりマシ
        // 後でSoundManagerEXのデストラクタで全て削除する
    }

    // 手動でクリーンアップする関数
    void Cleanup()
    {
        if (m_soundBuffer)
        {
            delete[] m_soundBuffer;
            m_soundBuffer = nullptr;
        }
        m_size = 0;
    }
};

class SoundSystem
{
public:
    SoundSystem();
    ~SoundSystem();

    bool LoadWaveFile(const std::string& a_wFilePath, WaveData* outData);
    int PlayWabeSound(const std::string& fileName, WaveData* outData, bool loop, float volume = 1.0f);
    void StopSound(int handle);  // 音を止める
    void StopAllSounds();  // 全ての音を止める
    void SetVolume(int handle, float volume);

private:
    IXAudio2* m_pAudio2;
    IXAudio2MasteringVoice* m_pMasterVoice;
    std::map<int, IXAudio2SourceVoice*> m_sourceVoices;  // ハンドルで管理
    int m_nextHandle;  // 次のハンドルID
    bool m_mediaFoundationInitialized;

    void CleanupFinishedVoices();
};

#endif // !___SOUND_H___
