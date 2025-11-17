#include "SweetsManager.h"

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

std::list<std::shared_ptr<Sweets>> SweetsManager::GetByState(SweetsState state)
{
	std::list<std::shared_ptr<Sweets>> ret;

	for (auto it : byState)
	{
		if (it.second == state)
		{
			ret.push_back(it.first);
		}

	}

	return ret;
}
