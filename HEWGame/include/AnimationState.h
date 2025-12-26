#pragma once

#include "Animation.h"
#include <string>
#include <memory>

class AnimationState
{
public:
	AnimationState();
	~AnimationState();

	std::shared_ptr<Animation> GetHasAnimation;

	//void SetState(std::string, std::shared_ptr<Animation>, std::string);

private:
};