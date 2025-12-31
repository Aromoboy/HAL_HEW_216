#include "SoundManagerEX.h"

SoundManagerEX* SoundManagerEX::s_instance = nullptr;

SoundManagerEX::SoundManagerEX()
{
    m_pSoundSystem = std::make_unique<SoundSystem>();
    SetInstance(this);
}

SoundManagerEX::~SoundManagerEX()
{
    // 全ての音を停止
    if (m_pSoundSystem)
    {
        m_pSoundSystem->StopAllSounds();
    }

    // SoundSystemを先に破棄（全てのSourceVoiceを破棄）
    m_pSoundSystem.reset();

    // その後でWaveDataをクリーンアップ
    for (auto& pair : m_cachedWaveDatas)
    {
        if (pair.second)
        {
            pair.second->Cleanup();
        }
    }
    m_cachedWaveDatas.clear();

    // グローバルインスタンスをクリア
    if (s_instance == this)
    {
        s_instance = nullptr;
    }
}


void SoundManagerEX::UpdateSound()
{
}

int SoundManagerEX::PlaySoundEX(std::string a_szSoundKey, bool a_isLoop, float volume)
{
    if (!m_pSoundSystem) return -1;

    std::shared_ptr<WaveData> waveData;
    auto it = m_cachedWaveDatas.find(a_szSoundKey);

    if (it == m_cachedWaveDatas.end())
    {
        waveData = std::make_shared<WaveData>();
        m_cachedWaveDatas[a_szSoundKey] = waveData;
    }
    else
    {
        waveData = it->second;
    }

    return m_pSoundSystem->PlayWabeSound(a_szSoundKey, waveData.get(), a_isLoop, volume);
}

void SoundManagerEX::StopSound(int handle)
{
    if (m_pSoundSystem)
    {
        m_pSoundSystem->StopSound(handle);
    }
}

void SoundManagerEX::SetVolume(int handle, float volume)
{
    if (m_pSoundSystem)
    {
        m_pSoundSystem->SetVolume(handle, volume);
    }
}


void SoundManagerEX::StopAllSounds()
{
    if (m_pSoundSystem)
    {
        m_pSoundSystem->StopAllSounds();
    }
}
