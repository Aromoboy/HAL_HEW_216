#pragma once

#include "Vector.h"
#include "Model.h"
#include <string>
#include <memory>




class TestObject
{
public:
	TestObject(std::string a_szModelKey, float3 a_fPosition, float3 a_fScale, float3 a_fRads);

	~TestObject() = default;

	const float3 GetPosition();
	const float3 GetScale();
	const float3 GetfRads();

	Model& GetModel();


private:
	std::unique_ptr<Model> m_pModel;

	float3 m_fPos;
	float3 m_fScale;
	float3 m_fRads;



};

