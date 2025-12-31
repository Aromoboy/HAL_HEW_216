#pragma once

#include "Animation.h"
#include <string>
#include <memory>
#include <unordered_map>

struct AnimationState
{
	std::shared_ptr<Animation> animation;
	size_t priority;
	std::string nextState;

	AnimationState()
		: animation(nullptr)
		, priority(-1)
		, nextState("NotAnimation")
	{ }
};

class AnimationStateMachine
{
public:
	AnimationStateMachine();
	~AnimationStateMachine();

	std::shared_ptr<Animation> GetHasAnimation();

	void SetState(std::string);

	void UpdateAnimation(float dt);

	void AddState(std::string, AnimationState state);

private:

	AnimationState currentState;

	std::unordered_map <std::string, AnimationState> animationStates;
};