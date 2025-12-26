#include "Animation.h"


void Animation::SetLoop(bool isLoop)
{
	m_info.isLoop = isLoop;
}

Animation::Animation(AnimePlayInfo a_info, std::vector<AnimeChannel> a_channel, Animation::AnimationArray a_array, std::vector<NodeDX> a_nodes)
	: m_info(a_info)
	, m_channels(a_channel)
	, m_animeTransform(a_array)
	, m_nodes(a_nodes)
	, isPlayEnd(false)
{
}



void Animation::Step(float a_tick)
{
	
	////--- アニメーション行列の更新
	//// パラメトリック
	//if (m_playNo == kParametricAnime || m_blendNo == kParametricAnime)
	//{
	//	CalcAnime(PARAMETRIC0, m_parametric[0]);
	//	CalcAnime(PARAMETRIC1, m_parametric[1]);
	//}
	// 
	
	// メインアニメ
	//if (m_playNo != kAnimeNone && m_playNo != kParametricAnime)
	CalcAnime(MAIN);


	//// ブレンドアニメ
	//if (m_blendNo != kAnimeNone && m_blendNo != kParametricAnime)
	//{
	//	CalcAnime(BLEND, m_blendNo);
	//}

	// アニメーション行列に基づいて骨行列を更新
	CalcBones(0, DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f));

	//--- アニメーションの時間更新
	// メインアニメ
	UpdateAnime(a_tick);
	//// ブレンドアニメ
	//if (m_blendNo != kAnimeNone)
	//{
	//	UpdateAnime(m_blendNo, a_tick);
	//	m_blendTime += a_tick;
	//	if (m_blendTime <= m_blendTime)
	//	{
	//		// ブレンドアニメの自動終了
	//		m_blendTime = 0.0f;
	//		m_blendTotalTime = 0.0f;
	//		m_playNo = m_blendNo;
	//		m_blendNo = kAnimeNone;
	//	}
	//}
	//// パラメトリック
	//if (m_playNo == kParametricAnime || m_blendNo == kParametricAnime)
	//{
	//	UpdateAnime(m_parametric[0], a_tick);
	//	UpdateAnime(m_parametric[1], a_tick);
	//}
}

void Animation::Play()
{
	isPlayEnd = false;
	m_info.nowTime = 0.0f;
}

bool Animation::IsPlayEnd()
{
	return isPlayEnd;
}

DirectX::XMMATRIX Animation::GetNodeMatrix(NodeIndexDX a_index)
{
	if (a_index < 0 || a_index >= m_nodes.size())
	{
		return DirectX::XMMatrixIdentity();
	}

	return m_nodes[a_index].mat;
}

void Animation::CalcAnime(AnimePatternDX kind)
{
	
	std::vector<AnimeChannel>::iterator channelIt = m_channels.begin();
	for (auto channelIt = m_channels.begin(); channelIt != m_channels.end(); ++channelIt)
	{
		// アニメーションで変更するボーンがなければスキップ

		// TODO ここ数値と-1の比較で本当にあってる？
		if (channelIt->node == kNodeNone) { continue; }

		// タイムラインの設定がなければスキップ
		AnimeTimeline& timeline = channelIt->timeline;
		if (timeline.empty()) { continue; }

		// ボーンの姿勢をタイムラインに沿って更新
		
		AnimeTransformDX& transform = m_animeTransform[kind][channelIt->node];
		if (timeline.size() <= 1)
		{
			// キーが一つしかないので値をそのまま使用
			transform = channelIt->timeline[0];
		}
		else
		{
			AnimeTimeline::iterator startIt = timeline.begin();
			if (m_info.nowTime <= startIt->first)
			{
				// 先頭キーよりも前の時間なら、先頭の値を使用
				transform = startIt->second;
			}
			else if (timeline.rbegin()->first <= m_info.nowTime)
			{
				// 最終キーよりも後の時間なら、最後の値を使用
				transform = timeline.rbegin()->second;
			}
			else
			{
				// 指定された時間を挟む2つのキーから、補間された値を計算
				AnimeTimeline::iterator nextIt = timeline.upper_bound(m_info.nowTime);
				startIt = nextIt;
				--startIt;
				float rate = (m_info.nowTime - startIt->first) / (nextIt->first - startIt->first);
				LerpTransform(&transform, startIt->second, nextIt->second, rate);
			}
		}
	}
}

void Animation::CalcBones(NodeIndexDX index, const DirectX::XMMATRIX parent)
{
	AnimeTransformDX transform; // 合成結果の格納先

	//// パラメトリックの合成
	//if (m_playNo == PARAMETRIC_ANIME || m_blendNo == PARAMETRIC_ANIME)
	//{
	//	LerpTransform(&transform,
	//		m_animeTransform[PARAMETRIC0][index],
	//		m_animeTransform[PARAMETRIC1][index],
	//		m_parametricBlend);

	//	// のちのブレンドとの合成のために、パラメトリックの結果を再生・ブレンドのいずれかに格納
	//	if (m_playNo == PARAMETRIC_ANIME) { m_animeTransform[MAIN][index] = transform; }
	//	if (m_blendNo == PARAMETRIC_ANIME) { m_animeTransform[BLEND][index] = transform; }
	//}

	//// ブレンドアニメの合成
	//if (m_blendNo != ANIME_NONE)
	//{
	//	LerpTransform(&transform,
	//		m_animeTransform[MAIN][index],
	//		m_animeTransform[BLEND][index],
	//		m_blendTime / m_blendTotalTime);
	//}
	//else
	
	// 合成の必要がなかったので、メインの情報をそのまま格納
	transform = m_animeTransform[MAIN][index];
	

	if (!m_info.isLoop && m_info.nowTime >= m_info.totalTime)
	{
		m_info.nowTime = m_info.totalTime;
		isPlayEnd = true;
		return;
	}
	// 該当ノードの姿勢行列を計算
	NodeDX& node = m_nodes[index];
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslationFromVector(DirectX::XMLoadFloat3(&transform.translate));
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&transform.quaternion));
	DirectX::XMMATRIX S = DirectX::XMMatrixScalingFromVector(DirectX::XMLoadFloat3(&transform.scale));
	node.mat = (S * R * T) * parent;

	// 子要素の姿勢を更新
	ChildrenDX::iterator it = node.children.begin();
	while (it != node.children.end())
	{
		CalcBones(*it, node.mat);
		++it;
	}
}

void Animation::UpdateAnime(float tick)
{
	//// アニメーションチェック
	//if (no == PARAMETRIC_ANIME) { return; }

	// アニメーション時間の更新
	
	m_info.nowTime = m_info.nowTime + m_info.speed * tick;
	if (m_info.isLoop && m_info.nowTime > m_info.totalTime)
	{
		m_info.nowTime = 0.0f;
	}

	//CheckAnimePlayLoop(anime.info);
}

void Animation::LerpTransform(AnimeTransformDX* pOut, const AnimeTransformDX& a, const AnimeTransformDX& b, float rate)
{
	DirectX::XMVECTOR vec[][2] = {
		{ DirectX::XMLoadFloat3(&a.translate),	DirectX::XMLoadFloat3(&b.translate) },
		{ DirectX::XMLoadFloat4(&a.quaternion),	DirectX::XMLoadFloat4(&b.quaternion) },
		{ DirectX::XMLoadFloat3(&a.scale),		DirectX::XMLoadFloat3(&b.scale) },
	};
	vec[0][0] = DirectX::XMVectorLerp(vec[0][0], vec[0][1], rate);
	vec[1][0] = DirectX::XMQuaternionSlerp(vec[1][0], vec[1][1], rate);
	vec[2][0] = DirectX::XMVectorLerp(vec[2][0], vec[2][1], rate);

	DirectX::XMStoreFloat3(&pOut->translate, vec[0][0]);
	DirectX::XMStoreFloat4(&pOut->quaternion, vec[1][0]);
	DirectX::XMStoreFloat3(&pOut->scale, vec[2][0]);
}

