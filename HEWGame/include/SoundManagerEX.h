// ============================================================================
// 概要: SoundManagerEX に関するインターフェースやデータ構造を宣言し、include から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#ifndef ___SOUND_MANAGER_EX___
#define ___SOUND_MANAGER_EX___

#include <memory>
#include <vector>
#include <string>
#include "SoundSystem.h"
class SoundManagerEX
{
public:


	SoundManagerEX();
	~SoundManagerEX();

	void UpdateSound();

	bool PlaySoundEX(std::string a_szSoundKey, bool a_isLoop);

private:
	std::vector<std::unique_ptr<WaveData>> m_pSoundDatas;
};

#endif