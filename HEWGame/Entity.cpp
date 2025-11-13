#include<iostream>
#include "Entity.h"



CEntity::CEntity(std::string a_tag, float3 pos, float3 rot, float3 scl)
{
	m_transform.position = { 0.0f,0.0f,0.0f };
	m_transform.rotation = { 0.0f,0.0f,0.0f };
	m_transform.scale    = { 1.0f,1.0f,1.0f };
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