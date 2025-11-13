#include<iostream>
#include "Entity.h"

CEntity::CEntity(std::string a_tag, float3 pos, float3 rot, float3 scl)
{
	m_transform.position = pos;  
	m_transform.rotation = rot;  
	m_transform.scale    = scl;
	tag = a_tag;
}

CEntity::~CEntity()
{

}

void CEntity::OnUpdate()
{

}

void CEntity::Draw(ModelRender renderer)
{

}

Transform& CEntity::GetTransform()
{
	return m_transform;
}