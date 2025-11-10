#ifndef ___SOUND_H___
#define ___SOUND_H___

#include <windows.h>

#pragma comment(lib, "xaudio2.lib")
#include <xaudio2.h>

#pragma comment(lib, "winmm.lib")
#include "mmsystem.h"

#include <string>
#include <memory>


struct WaveData
{
	WAVEFORMATEX m_wavFormat;
	char* m_soundBuffer;
	DWORD m_size;

	WaveData()
	{
		m_soundBuffer = nullptr;
	}
	~WaveData() { if (m_soundBuffer)delete m_soundBuffer; }
};

class SoundSystem
{
public:
	SoundSystem();
	~SoundSystem();



	
	bool LoadWaveFile(const std::string& a_wFilePath, WaveData* outData);

	bool PlayWabeSound(const std::string& fileName, WaveData* outData, bool loop);

private:
	IXAudio2* m_pAudio2;
	IXAudio2MasteringVoice* m_pMasterVoice;
	IXAudio2SourceVoice* m_pSourceVoice;

};





#endif // !___SOUND_H___

