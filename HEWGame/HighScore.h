#pragma once
class HighScore
{
public:
	bool HighScoreCompare(int);//比較
	void WriteHighscore(int);//書き込み
	int GetHighscore();//値を渡す
private:
	int m_nCompHighScore;
	int m_nHighScore;
	//string m_szHighScore;
};

