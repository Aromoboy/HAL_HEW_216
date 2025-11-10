
#ifndef ___ENTITY_H___
#define ___ENTITY_H___

#include <string>
using namespace std;

class CEntity
{
public:
	void Update();
	void Draw();
	void GetTransform();
private:
	int Transform;
	string tag;
};

#endif 