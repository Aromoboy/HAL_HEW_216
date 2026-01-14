#include "HighScore.h"
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

//比較
bool HighScore::HighScoreCompare(int nNewScore)
{
    int nCompHighScore = GetHighscore();       //ハイスコア取得
    if (nNewScore < nCompHighScore)
        return false;

    else
        return true;
}

//書き込み
void HighScore::WriteHighscore(int nWriteHighScore)
{
    ofstream ofs("HighScore.txt");  // 書き込み用

    if (!ofs) {
        cerr << "ファイルを開けません\n";
        return ;
    }

    ofs << nWriteHighScore;     //書き込み

    ofs.close();  // 明示的に閉じる（省略可）
}

//値取得
int HighScore::GetHighscore()
{
    ifstream ifs("HighScore.txt");  // 読み込み用

    if (ifs) 
    {
        string szHighScore;
        getline(ifs, szHighScore);          //ファイルから取得
        m_nHighScore = stoi(szHighScore);   //文字列を数値に変換
        return m_nHighScore;
    }

    else 
        return 0;   //取得できなければ0を返す  
}
