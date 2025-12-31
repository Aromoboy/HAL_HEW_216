// エンジンのモデルアニメーションのデータを扱う
// アニメーション再生と描画から参照される

#pragma once

#include "DirectX.h"
#include "ModelDX.h"
#include <array>

#include <map>

using AnimeNoDX = int;
using MorphNoDX = int;	// [tf[^ԍ

using AnimeTime = float;
constexpr AnimeNoDX	 kAnimeNone = -1;	// YAj[VȂ
static const MorphNoDX  kMorphNone = -1;	// Y[tȂ
static const AnimeNoDX  kParametricAnime = -2;	// Aj[V


class Animation
{
public:
	//===== Aj[V =====

	// Aj[VvZ̈
	struct AnimeTransformDX
	{
		DirectX::XMFLOAT3	translate;
		DirectX::XMFLOAT4	quaternion;
		DirectX::XMFLOAT3	scale;
	};

	enum AnimePatternDX
	{
		MAIN,			// ʏĐ
		BLEND,			// uhĐ
		PARAMETRIC0,	// A
		PARAMETRIC1,	// B
		MAX_ANIMEPATTERN
	};

	using AnimeKey = std::pair<AnimeTime, AnimeTransformDX>;
	using AnimeTimeline = std::map<AnimeTime, AnimeTransformDX>;
	using AnimeTransforms = std::vector<AnimeTransformDX>;
	using AnimationArray = std::array<std::vector<AnimeTransformDX>, MAX_ANIMEPATTERN>;

	// ꎞIɂŐݒA܂̓I点Ȃ
	void SetLoop(bool isLoop);

	// Aj[VĐ
	struct AnimePlayInfo
	{
		AnimeTime		nowTime;	// ݂̍Đ
		AnimeTime		totalTime;	// őĐ
		float			speed;		// Đx
		bool			isLoop;		// [vw
	};


	// Aj[Vƃm[h̊֘At
	struct AnimeChannel
	{
		NodeIndexDX		node;		// Ήm[h
		AnimeTimeline	timeline;	// m[hɕtAj[V
	};



	// RXgN^AfXgN^
	Animation(AnimePlayInfo a_info, std::vector<AnimeChannel> a_channel, Animation::AnimationArray a_array, std::vector<NodeDX> a_nodes);
	~Animation() = default;

	// OnUpdateŕێĂAj[VXV
	
	void Step(float a_tick);

	void Play();

	bool IsPlayEnd();

	DirectX::XMMATRIX GetNodeMatrix(NodeIndexDX);

private:

	AnimePlayInfo	m_info;		// Đ
	std::vector<AnimeChannel>	m_channels;	// ϊ

	std::vector<NodeDX> m_nodes; // ݂Bone̕ϊs

	void CalcAnime(AnimePatternDX kind);

	// Đ@ʂ̍s񌋉ʂ̍
	void CalcBones(NodeIndexDX index, const DirectX::XMMATRIX parent);

	void UpdateAnime(float tick);

	AnimationArray m_animeTransform;

	bool isPlayEnd;

	void LerpTransform(AnimeTransformDX* pOut, const AnimeTransformDX& a, const AnimeTransformDX& b, float rate);
	
};


