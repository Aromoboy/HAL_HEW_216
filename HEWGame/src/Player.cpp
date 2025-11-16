#include "Player.h"
#include "Core/Input.h"
#include "ModelCache.h"

#define PLAYER_KEY_MOVE_UP 0x57		// W
#define PLAYER_KEY_MOVE_LEFT 0x41	// A
#define PLAYER_KEY_MOVE_DOWN 0x53	// S
#define PLAYER_KEY_MOVE_RIGHT 0x44	// D

#define PlayerSpeed (1.0f)


Player::Player(std::string a_tag, float3 pos, float3 rot, float3 scl, std::shared_ptr<Collision::ICollider> collide, Core::UpdatePhase phase, int priority)
	: CollisionEntity(a_tag, pos, rot, scl, collide, phase, priority), model(nullptr)
{
	Render::ModelCache& cache = Render::ModelCache::Instance();
	model = cache.Get("Assets/Model/LowPolyNature/Models/Flowers_01.fbx");
}

void Player::OnUpdate(Core::UpdateContext& ctx)
{
	Core::Input& input = ctx.input;

	float vectorX = 0.0f;
	float vectorY = 0.0f;

	if(input.IsKeyTrigger(PLAYER_KEY_MOVE_UP))
	{
		vectorY =  PlayerSpeed;
	}
	else if (input.IsKeyTrigger(PLAYER_KEY_MOVE_DOWN))
	{
		vectorY = -PlayerSpeed;
	}

	if (input.IsKeyTrigger(PLAYER_KEY_MOVE_LEFT))
	{
		vectorX = -PlayerSpeed;
	}
	else if (input.IsKeyTrigger(PLAYER_KEY_MOVE_RIGHT))
	{
		vectorX =  PlayerSpeed;
	}
	

	// ãÉÇ´Ç»Ç™ÇÁòlÇ—Ç‹Ç∑ÅB
	m_transform.position.x += vectorX;
	m_transform.position.y += vectorY;
	

}

void Player::Draw(ModelRender render, ICamera& camera)
{
	render.Draw(*model, camera);
}
