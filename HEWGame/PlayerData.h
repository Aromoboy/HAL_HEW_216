//プレイヤーデータの保存
//保存する内容(仕様書より)
//プレイヤーのスピード倍率
//クリームの解放
//フルーツの補充速度
//補充されるフルーツの数
//ポイントの取得倍率
//ケーキのサイズ 
//所持ポイント
#pragma once
#include <fstream>
#include <sstream>
#include <vector>
using namespace std;

struct  Data
{
	float fSpeed = 1.0f;	//スピード(PlayerControllerのresult.speedMultiplierあたり)
	bool bCream = false;	//クリームの解放
	float fCakeSize;		//ケーキのサイズ
	float fReplenishSpeed;	//フルーツの補充速度
	int nReplenishCount;	//補充されるフルーツの数
	float fPtMultipl;		//ポイントの取得倍率
	int nPoints = 0;		//所持ポイント
};

class PlayerData
{
public:		//書き込み、読み込み取得
	~PlayerData();
	void SaveData(string& fileName, vector<Data>& PlayerData);
	vector<Data> LoadData(string fileName);
private:
	PlayerData(){}
	PlayerData(const PlayerData& PlayerData){}
	PlayerData& operator=(const PlayerData& PlayerData) { return *this; }
	Data Player;
};

