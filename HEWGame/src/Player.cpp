#include "Player.h"
#include "Core/Input.h"
#include "ModelCache.h"
#include "Physical.h"

#define PLAYER_KEY_MOVE_UP 0x57		// W
#define PLAYER_KEY_MOVE_LEFT 0x41	// A
#define PLAYER_KEY_MOVE_DOWN 0x53	// S
#define PLAYER_KEY_MOVE_RIGHT 0x44	// D

#define PlayerSpeed (0.25f)

SceneSpace::Player::Player(std::string a_tag,
	float3 pos,
	float3 rot,
	float3 scl,
	std::shared_ptr<Collision::ICollider> collide,
	float3 velocity,
	float3 force,
	float weight,
	float damping,
	Core::UpdatePhase phase,
	int priority)
	: PhysicalEntity(a_tag, pos, rot, scl, collide, velocity, force, weight, damping, phase, priority), model(nullptr)
{
	Render::ModelCache& cache = Render::ModelCache::Instance();
	model = cache.Get("Assets/Model/LowPolyNature/Models/Flowers_01.fbx");
}

void SceneSpace::Player::OnUpdate(Core::UpdateContext& ctx)
{
        Core::Input& input = ctx.input;

        float vectorX = 0.0f;
        float vectorY = 0.0f;

        if (input.IsKeyPress(PLAYER_KEY_MOVE_UP))
        {
                vectorY = PlayerSpeed;
        }
        else if (input.IsKeyPress(PLAYER_KEY_MOVE_DOWN))
        {
                vectorY = -PlayerSpeed;
        }

        if (input.IsKeyPress(PLAYER_KEY_MOVE_LEFT))
        {
                vectorX = -PlayerSpeed;
        }
        else if (input.IsKeyPress(PLAYER_KEY_MOVE_RIGHT))
        {
                vectorX = PlayerSpeed;
        }

        if (vectorX != 0.0f || vectorY != 0.0f)
        {
                ApplyTranslation({ vectorX, vectorY, 0.0f });
        }


}

void SceneSpace::Player::Draw(ModelRender& render, CameraSpace::ICamera& camera)
{
        render.Draw(*model, m_transform, &camera);
}

ModelDX* SceneSpace::Player::GetRenderableModel()
{
        return model.get();
}

const ModelDX* SceneSpace::Player::GetRenderableModel() const
{
        return model.get();
}


void SceneSpace::Player::HandleInput(Core::Input input)
{
}
