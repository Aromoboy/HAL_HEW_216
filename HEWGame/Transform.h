#ifndef ___TRANSFORM_H___
#define ___TRANSFORM_H___

#include <string>
#include <Vector.h>
using namespace std;

struct Transform
{
	float3 position;
	float3 rotation;
	float3 scale;
};

Transform m_Transform;
std::string tag;

#endif