#ifndef ___ENTITY_H___
#define ___ENTITY_H___

#include "Transform.h"
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

	void Update(float);
	void Draw();
	Transform GetTransform() { return  Components_Transform; }
	
private:
	
	
};

#endif 