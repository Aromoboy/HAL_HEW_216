// ============================================================================
// 概要: SoundSystem に関するインターフェースやデータ構造を宣言し、include から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

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





#endif // 備考: !___SOUND_H___ に関する説明を日本語で補足している。

