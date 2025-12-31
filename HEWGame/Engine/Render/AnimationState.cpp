#include "AnimationState.h"
#include "Animation.h"

AnimationStateMachine::AnimationStateMachine()
{
	animationStates["NotAnimation"] = AnimationState();

	currentState = animationStates["NotAnimation"];
}

AnimationStateMachine::~AnimationStateMachine()
{
}

std::shared_ptr<Animation> AnimationStateMachine::GetHasAnimation()
{
	return currentState.animation;
}

void AnimationStateMachine::SetState(std::string a_stateKey)
{
	AnimationState nextState = animationStates[a_stateKey];

	if (nextState.priority >= currentState.priority)
	{
		currentState = nextState;
	}
}

void AnimationStateMachine::UpdateAnimation(float dt)
{
	if (!currentState.animation) return;
	currentState.animation->Step(dt);


}

void AnimationStateMachine::AddState(std::string a_stateKey, AnimationState state)
{
	animationStates[a_stateKey] = state;
}
