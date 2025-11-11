#ifndef ___ENTITY_H___
#define ___ENTITY_H___

#include "Transform.h"
#include "ModelRender.h"
#include <string>
#include <Vector.h>


class CEntity
{
public:
	//コンストラクタ
	CEntity(std::string a_tag);
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