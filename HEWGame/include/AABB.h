#ifndef ___AABB_H___
#define ___AABB_H___

#include "Vector.h"

struct AABB
{
	//ヒットボックスの最大値と最小値
	float3 max;
	float3 min;

	AABB(float3 a_max, float3 a_min)
	{
		max = a_max;
		min = a_min;
	};

	AABB() 
	{
		AABB::max = float3{ 0.0f,0.0f,0.0f };
		AABB::min = float3{ 0.0f,0.0f,0.0f };
	};


};

#endif