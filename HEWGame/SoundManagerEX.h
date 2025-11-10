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