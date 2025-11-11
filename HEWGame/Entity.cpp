#include<iostream>
#include "Entity.h"



CEntity::CEntity()
{

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