#ifndef ___ENTITY_H___
#define ___ENTITY_H___

#include "Transform.h"
#include "include/ModelRender.h"
#include <string>

class CEntity
{
public:
	//コンストラクタ
	CEntity(std::string a_tag, float3& pos, float3& rot, float3& scl);
	//デストラクタ
	~CEntity();

	void OnUpdate();
	void Draw(ModelRender);
	Transform& GetTransform();
	
private:
	Transform m_transform;
	std::string tag;
	
};

#endif 