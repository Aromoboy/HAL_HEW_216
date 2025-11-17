#pragma once


#include <unordered_map>
#include <memory>
#include "Core/UpdateNode.h"
#include "ModelRender.h"
#include <list>
#include <string>
#include "Transform.h"
#include "functional"

class BaseSweets;

enum SweetsState
{
	Active,
	Staked,
	Ground

};

class SweetsManager : Core::UpdateNode
{
public:
	SweetsManager();
	~SweetsManager();

	virtual void OnUpdate(Core::UpdateContext& ctx) override;

	void DrawAll(ModelRender render);

	std::list<std::shared_ptr<BaseSweets>> GetByState(SweetsState state);

	std::shared_ptr<BaseSweets> Spawn(std::string kind,  Transform transform);

private:
	std::unordered_map<std::shared_ptr<BaseSweets>, SweetsState> byState;

	std::unordered_map<std::string, std::function<std::shared_ptr<BaseSweets>(Transform)>> spawner;
	// spawner‚É‚½‚¹‚éì¬ŠÖ”
	template<typename T> 
	std::shared_ptr<BaseSweets> Make_Sweets(Transform transform)
	{
		return std::make_shared<T>(transform.position, transform.rotation, transform.scale /*etx...*/);
	}
};