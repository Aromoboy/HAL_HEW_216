#pragma once
class Timer
{
public:
	Timer();
	~Timer();

	void StepTime(float a_deltatime);	//deltatime分時間を減らしていく
	bool GetCount();					//現在の時間を取得
	void ResetTime();					//リセット
private:
	float m_maxTime;

};

