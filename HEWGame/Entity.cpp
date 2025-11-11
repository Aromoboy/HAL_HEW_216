#include<iostream>
#include "Entity.h"



CEntity::CEntity(std::string a_tag)
{
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