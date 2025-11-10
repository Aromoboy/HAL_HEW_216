#ifndef ___TRANSFORM_H___
#define ___TRANSFORM_H___

#include <string>
#include <Vector.h>
using namespace std;

struct Transform
{
	float3 Position;
	float3 Rotation;
	float3 Scale;
};

Transform m_Transform;
string tag;

#endif