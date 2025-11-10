#include "SoundSystem.h"
#include "Debug.h"


#ifdef _XBOX //Big-Endian
#define fourccRIFF 'RIFF'
#define fourccDATA 'data'
#define fourccFMT 'fmt '
#define fourccWAVE 'WAVE'
#define fourccXWMA 'XWMA'
#define fourccDPDS 'dpds'
#endif

#ifndef _XBOX //Little-Endian
#define fourccRIFF 'FFIR'
#define fourccDATA 'atad'
#define fourccFMT ' tmf'
#define fourccWAVE 'EVAW'
#define fourccXWMA 'AMWX'
#define fourccDPDS 'sdpd'
#endif



SoundSystem::SoundSystem()
{
#ifdef _DEBUG
	DebugMessage("SoundSystemは制作中です、使用しないでください。");
#endif



	m_pAudio2 = nullptr;
	m_pMasterVoice = nullptr;
	m_pSourceVoice = nullptr;

    HRESULT result;
    result = CoInitializeEx(NULL, COINIT_MULTITHREADED);


    if (FAILED(result))
    {
#ifdef _DEBUG
        DebugMessage("COMの初期化に失敗");
#endif // _DEBUG
        return;
    }




    // XAudio2の初期化
    if (FAILED(result = XAudio2Create(&m_pAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR)))
    {
#ifdef _DEBUG
        DebugMessage("XAudioの作成に失敗しました。");
#endif
        return;
    }

	if (FAILED(result = m_pAudio2->CreateMasteringVoice(&m_pMasterVoice))) 
    {
#ifdef _DEBUG
        DebugMessage("XAudioの作成に失敗しました。");
#endif
        return;
    }




    
}

SoundSystem::~SoundSystem()
{
	if(m_pSourceVoice) m_pSourceVoice->DestroyVoice();
    if(m_pMasterVoice) m_pMasterVoice->DestroyVoice();
    if(m_pAudio2) m_pAudio2->Release();

    CoUninitialize();
}




bool SoundSystem::LoadWaveFile(const std::string& a_wFilePath, WaveData* outData)
{
	// 何か入っていると困るため初期化
	if (outData)
	{
		free(outData->m_soundBuffer);
	}
	// nullptrなら返す
	else return false;

	HMMIO mmioHandle = nullptr;

	// チャンク情報
	MMCKINFO chunkInfo{};

	// RIFFチャンク用
	MMCKINFO riffChunkInfo{};

	// WAVファイルを開く
	mmioHandle = mmioOpen(
		(LPSTR)a_wFilePath.data(),
		nullptr,
		MMIO_READ
	);

	if (!mmioHandle) return false;

	riffChunkInfo.fccType = mmioFOURCC('W', 'A', 'V', 'E');

	// RIFFチャンクに侵入する
	if (mmioDescend(
		mmioHandle,		// MMIOハンドル
		&riffChunkInfo,	// 取得したチャンクの情報
		nullptr,		// 親チャンク
		MMIO_FINDRIFF	// 取得情報の種類
	) != MMSYSERR_NOERROR)
	{
		// Riffチャンクに侵入失敗
#ifdef _DEBUG
		DebugMessage("Sound.cpp:Riffチャンクに侵入失敗しました。");
#endif
		mmioClose(mmioHandle, MMIO_FHOPEN);
		return false;
	}

	// 侵入先のチャンクを"fmt "として設定する
	chunkInfo.ckid = mmioFOURCC('f', 'm', 't', ' ');
	if (mmioDescend(
		mmioHandle,
		&chunkInfo,
		&riffChunkInfo,
		MMIO_FINDCHUNK
	) != MMSYSERR_NOERROR)
	{
		// fmtチャンクがない
#ifdef _DEBUG
		DebugMessage("Sound.cpp:fmtチャンクが存在しません。");
#endif
		mmioClose(mmioHandle, MMIO_FHOPEN);
		return false;
	}

	DWORD readSize = mmioRead(
		mmioHandle,						// ハンドル
		(HPSTR)&outData->m_wavFormat,	// 読み込み用バッファ
		chunkInfo.cksize				// バッファサイズ
	);

	if (readSize != chunkInfo.cksize)
	{
		// 読み込みサイズが一致していません
#ifdef _DEBUG
		DebugMessage("Sound.h:読み込みサイズが一致していません。");
#endif
		mmioClose(mmioHandle, MMIO_FHOPEN);
		return false;
	}

	// フォーマットチェック
	if (outData->m_wavFormat.wFormatTag != WAVE_FORMAT_PCM)
	{
		// waveフォーマットエラーです

#ifdef _DEBUG
		DebugMessage("Sound.cpp:waveフォーマットエラーです");
#endif
		mmioClose(mmioHandle, MMIO_FHOPEN);
		return false;
	}

	// fmtチャンクを退出する
	if (mmioAscend(mmioHandle, &chunkInfo, 0) != MMSYSERR_NOERROR)
	{
		// fmtチャンク退出失敗
#ifdef _DEBUG
		DebugMessage("Sound.h:fmtチャンクからの退出に失敗しました。");
#endif
		mmioClose(mmioHandle, MMIO_FHOPEN);
		return false;
	}

	// dataチャンクに侵入
	chunkInfo.ckid = mmioFOURCC('d', 'a', 't', 'a');
	if (mmioDescend(mmioHandle, &chunkInfo, &riffChunkInfo, MMIO_FINDCHUNK) != MMSYSERR_NOERROR)
	{
		// dataチャンク侵入失敗
#ifdef _DEBUG
		DebugMessage("Sound.cpp:dataチャンクへの侵入に失敗しました。");
#endif
		mmioClose(mmioHandle, MMIO_FHOPEN);
		return false;
	}

	// サイズ保存
	outData->m_size = chunkInfo.cksize;

	// dataチャンク読み込み
	outData->m_soundBuffer = new char[chunkInfo.cksize];
	readSize = mmioRead(mmioHandle, (HPSTR)outData->m_soundBuffer, chunkInfo.cksize);
	if (readSize != chunkInfo.cksize)
	{
		// dataチャンク読み込み失敗
#ifdef _DEBUG
		DebugMessage("Sound.cpp:dataチャンクの読み込みに失敗しました。");
#endif
		mmioClose(mmioHandle, MMIO_FHOPEN);
		delete[] outData->m_soundBuffer;
		return false;
	}

	mmioClose(mmioHandle, MMIO_FHOPEN);

	return true;
}


bool SoundSystem::PlayWabeSound(const std::string& fileName, WaveData* outData, bool loop)
{
	if (!LoadWaveFile(fileName, outData))
	{
		// Waveファイル読み込み失敗
		
		return false;
	}

	//SourceVoiceの作成
	WAVEFORMATEX waveFormat{};

	// 波形フォーマットの設定
	memcpy(&waveFormat, &outData->m_wavFormat, sizeof(outData->m_wavFormat));

	// 1サンプル当たりのバッファサイズを算出
	waveFormat.wBitsPerSample = outData->m_wavFormat.nBlockAlign * 8 / outData->m_wavFormat.nChannels;

	// ソースボイスの作成 ここではフォーマットのみ渡っている
	HRESULT result = m_pAudio2->CreateSourceVoice(&m_pSourceVoice, (WAVEFORMATEX*)&waveFormat);
	if (FAILED(result))
	{
		// SourceVoice作成失敗
		DebugMessage("SourceVoiceの作成に失敗しました。");
		return false;
	}


	// 波形データ(音データ本体をソースボイスに渡す)

	XAUDIO2_BUFFER xAudio2Buffer{};
	xAudio2Buffer.pAudioData = (BYTE*)outData->m_soundBuffer;
	xAudio2Buffer.Flags = XAUDIO2_END_OF_STREAM;
	xAudio2Buffer.AudioBytes = outData->m_size;

	// 三項比較演算子を用いてループするか否かの設定をする
	xAudio2Buffer.LoopCount = loop ? XAUDIO2_LOOP_INFINITE : 0;

	m_pSourceVoice->SubmitSourceBuffer(&xAudio2Buffer);

	// 実際に音を鳴らす
	m_pSourceVoice->Start();

	return true;
}

