#ifndef ___ENTITY_H___
#define ___ENTITY_H___

#include "Transform.h"
#include "ModelRender.h"
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

	void OnUpdate();
	void Draw(ModelRender);
	Transform GetTransform() { return  m_transform; }
	
private:
	std::string tag;
	
};

#endif 