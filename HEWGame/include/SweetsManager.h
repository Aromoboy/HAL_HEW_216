#pragma once


#include <unordered_map>
#include <memory>
#include "Core/UpdateNode.h"
#include "ModelRender.h"
#include <list>

class Sweets;

enum SweetsState
{
	Active,
	Staked,
	Ground

};

class SweetsManager : Core::UpdateNode
{
public:
	virtual void OnUpdate(Core::UpdateContext& ctx) override;

	void DrawAll(ModelRender render);

	std::list<std::shared_ptr<Sweets>> GetByState(SweetsState state);


private:
	std::unordered_map<std::shared_ptr<Sweets>, SweetsState> byState;
};