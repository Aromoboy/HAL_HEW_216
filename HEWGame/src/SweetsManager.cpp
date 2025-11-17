#include "SweetsManager.h"


SweetsManager::SweetsManager()
{
	spawner["Cake"] = &Make_Sweets<int>;
}

SweetsManager::~SweetsManager()
{
}

void SweetsManager::OnUpdate(Core::UpdateContext& ctx)
{

}

void SweetsManager::DrawAll(ModelRender render)
{
	for (auto it : byState)
	{
		// it.first // •`‰æ
	}
}

std::list<std::shared_ptr<BaseSweets>> SweetsManager::GetByState(SweetsState state)
{
	std::list<std::shared_ptr<BaseSweets>> ret;

	for (auto it : byState)
	{
		if (it.second == state)
		{
			ret.push_back(it.first);
		}

	}

	return ret;
}

std::shared_ptr<BaseSweets> SweetsManager::Spawn(std::string kind, Transform transform)
{
	return spawner.at(kind)(transform);
}


