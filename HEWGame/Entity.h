
#ifndef ___ENTITY_H___
#define ___ENTITY_H___

#include <string>
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
	int GetTransform() { return Transform; }
private:
	int Transform;
	string tag;
};

#endif 