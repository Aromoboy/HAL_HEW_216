#ifndef ___SOUND_MANAGER_EX___
#define ___SOUND_MANAGER_EX___

#include <map>
#include <string>
#include <memory>
#include "SoundSystem.h"

class SoundManagerEX
{
public:
    SoundManagerEX();
    ~SoundManagerEX();

    void UpdateSound();
    int PlaySoundEX(std::string a_szSoundKey, bool a_isLoop, float volume = 1.0f);  // int‚É•ÏX
    void StopSound(int handle);  // ‰¹‚ğ~‚ß‚é
    void StopAllSounds();  // ‘S‚Ä‚Ì‰¹‚ğ~‚ß‚é
    void SetVolume(int handle, float volume);

    static SoundManagerEX* GetInstance() { return s_instance; }
    static void SetInstance(SoundManagerEX* instance) { s_instance = instance; }

private:
    std::unique_ptr<SoundSystem> m_pSoundSystem;
    std::map<std::string, std::shared_ptr<WaveData>> m_cachedWaveDatas;

    static SoundManagerEX* s_instance;
};

#endif
