#pragma once

#include "Core/UpdateNode.h"


class UpdatableTimer : public Core::UpdateNode
{
public:
	UpdatableTimer();

	virtual void OnUpdate(Core::UpdateContext& ctx);

	float GetCount();
	void SetCount(float);
	void AddCount(float);

private:
	float m_count;
};