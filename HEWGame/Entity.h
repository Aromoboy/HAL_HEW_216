#ifndef ___ENTITY_H___
#define ___ENTITY_H___


#include <string>
#include <Vector.h>
using namespace std;

class CEntity
{
public:
	//コンストラクタ
	CEntity();
	//デストラクタ
	~CEntity();

	void Update();
	void Draw();
	float3 GetTransform() { return struct Transform; }
private:
	struct Transform
	{
		float3 Position;
		float3 Rotation;
		float3 Scale;

	}; 
	string tag;
};

#endif 