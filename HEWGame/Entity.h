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


	
private:
	Transform m_Transform;
	
};

#endif 