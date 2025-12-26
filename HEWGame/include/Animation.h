#pragma once

#include "DirectX.h"
#include "ModelDX.h"
#include <array>

#include <map>

using AnimeNoDX = int;
using MorphNoDX = int;	// モーフデータ番号

using AnimeTime = float;
constexpr AnimeNoDX	 kAnimeNone = -1;	// 該当アニメーションなし
static const MorphNoDX  kMorphNone = -1;	// 該当モーフなし
static const AnimeNoDX  kParametricAnime = -2;	// 合成アニメーション

class Animation
{
public:
	//===== アニメーション =====

	// アニメーション計算領域
	struct AnimeTransformDX
	{
		DirectX::XMFLOAT3	translate;
		DirectX::XMFLOAT4	quaternion;
		DirectX::XMFLOAT3	scale;
	};

	enum AnimePatternDX
	{
		MAIN,			// 通常再生
		BLEND,			// ブレンド再生
		PARAMETRIC0,	// 合成A
		PARAMETRIC1,	// 合成B
		MAX_ANIMEPATTERN
	};

	using AnimeKey = std::pair<AnimeTime, AnimeTransformDX>;
	using AnimeTimeline = std::map<AnimeTime, AnimeTransformDX>;
	using AnimeTransforms = std::vector<AnimeTransformDX>;
	using AnimationArray = std::array<std::vector<AnimeTransformDX>, MAX_ANIMEPATTERN>;

	

	// アニメーション再生情報
	struct AnimePlayInfo
	{
	void ResetTime();
		AnimeTime		nowTime;	// 現在の再生時間
		AnimeTime		totalTime;	// 最大再生時間
		float			speed;		// 再生速度
		bool			isLoop;		// ループ指定
	};


	// アニメーションとノードの関連付け情報
	struct AnimeChannel
	{
		NodeIndexDX		node;		// 対応するノード
		AnimeTimeline	timeline;	// ノードに付随するアニメーション情報
	};



	// コンストラクタ、デストラクタ
	Animation(AnimePlayInfo a_info, std::vector<AnimeChannel> a_channel, Animation::AnimationArray a_array, std::vector<NodeDX> a_nodes);
	~Animation() = default;

	// OnUpdateで保持しているアニメーションを更新する
	
	void Step(float a_tick);

	DirectX::XMMATRIX GetNodeMatrix(NodeIndexDX);

private:

	AnimePlayInfo	m_info;		// 再生情報
	std::vector<AnimeChannel>	m_channels;	// 変換情報

	std::vector<NodeDX> m_nodes; // 現在のBoneの変換行列

	void CalcAnime(AnimePatternDX kind);

	// 再生方法別の行列結果の合成
	void CalcBones(NodeIndexDX index, const DirectX::XMMATRIX parent);

	void UpdateAnime(float tick);

	AnimationArray m_animeTransform;

	void LerpTransform(AnimeTransformDX* pOut, const AnimeTransformDX& a, const AnimeTransformDX& b, float rate);
	
};