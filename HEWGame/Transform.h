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


void Update();
void Draw();
Transform GetTransform() { return  m_Transform; }

Transform m_Transform;
string tag;

#endif