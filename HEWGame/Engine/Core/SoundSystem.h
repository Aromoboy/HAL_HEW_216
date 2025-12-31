// 音声の更新と再生処理をまとめている
// 更新フェーズから呼ばれて音が鳴る

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

    // fXgN^Ńobt@폜ȂiXAudio2gp̉\邽߁j
    ~WaveData()
    {
        // Ȃ - [N邪ANbV}V
        // SoundManagerEX̃fXgN^őSč폜
    }

    // 蓮ŃN[Abv֐
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
    int PlayWabeSound(const std::string& fileName, WaveData* outData, bool loop, float volume = 1.0f);  // intɕύXinhԂj
    void StopSound(int handle);  // ~߂
    void StopAllSounds();  // SẲ~߂
    void SetVolume(int handle, float volume);

private:
    IXAudio2* m_pAudio2;
    IXAudio2MasteringVoice* m_pMasterVoice;
    std::map<int, IXAudio2SourceVoice*> m_sourceVoices;  // nhŊǗ
    int m_nextHandle;  // ̃nhID

    void CleanupFinishedVoices();
};

#endif // !___SOUND_H___
