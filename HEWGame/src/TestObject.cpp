#include "TestObject.h"
#include "Debug.h"

TestObject::TestObject(std::string a_szModelKey, float3 a_fPosition, float3 a_fScale, float3 a_fRads): 
	m_fPos(a_fPosition),
	m_fScale(a_fScale),
	m_fRads(a_fRads)

{
	m_pModel = std::make_unique<Model>();

	std::string modelName = "Assets/Model/LowPolyNature/Models/" + a_szModelKey + ".fbx";

	if (!m_pModel->Load(modelName.c_str(), 0.3f, Model::Flip::None))
	{
#ifdef _DEBUG
		DebugMessage(a_szModelKey + "が読み込めませんでした。");
#endif // _DEBUG
	}

}

const float3 TestObject::GetPosition()
{
	return m_fPos;
}

const float3 TestObject::GetScale()
{
	return m_fScale;
}

const float3 TestObject::GetfRads()
{
	return m_fRads;
}

Model& TestObject::GetModel()
{
	return *m_pModel;
}

