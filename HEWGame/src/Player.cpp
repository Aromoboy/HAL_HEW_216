#include "Player.h"
#include "Core/Input.h"
#include "ModelCache.h"
#include "Physical.h"
#include "Sweets.h"
#include "SweetsManager.h"
#include "SweetsPrototypeManifest.h"

#include <algorithm>
#include <sstream>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

#define PLAYER_KEY_MOVE_UP 0x57		// W
#define PLAYER_KEY_MOVE_LEFT 0x41	// A
#define PLAYER_KEY_MOVE_DOWN 0x53	// S
#define PLAYER_KEY_MOVE_RIGHT 0x44	// D

#define PlayerSpeed (0.25f)
#define PLAYER_KEY_PICKUP 0x45       // E
#define PLAYER_KEY_DROP 0x51         // Q

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
    void PlayerDebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    void PlayerDebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

    constexpr const char* kPlayerLogPrefix = "[Player] ";
    constexpr const char* kFairyFallbackPath = "Assets/Model/SweetsPrototype/FairyPlaceholder.obj";
    constexpr float kDefaultPickupRadius = 2.5f;
    constexpr float kCarryVerticalOffset = 1.0f;
    constexpr float kCarryForwardOffset = 0.5f;
}

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
        : PhysicalEntity(a_tag, pos, rot, scl, collide, velocity, force, weight, damping, phase, priority)
        , model(nullptr)
        , m_sweetsManager(nullptr)
        , m_carried()
        , m_pickupRadius(kDefaultPickupRadius)
        , m_carryOffset(float3{ 0.0f, kCarryVerticalOffset, kCarryForwardOffset })
        , m_dropUpwardBoost(4.0f)
{
        Render::ModelCache& cache = Render::ModelCache::Instance();
        const auto& manifest = SweetsSpace::SweetsPrototypeManifest::Instance();
        const auto* descriptor = manifest.Find(SweetsSpace::SweetsType::Fairy);
        std::string modelPath = descriptor ? descriptor->modelPath : std::string(kFairyFallbackPath);

        std::ostringstream oss;
        if (descriptor)
        {
            oss << kPlayerLogPrefix << "Using fairy prototype '" << descriptor->key
                << "' asset='" << modelPath << "'.\n";
        }
        else
        {
            oss << kPlayerLogPrefix << "Fairy prototype missing from manifest. Falling back to '"
                << modelPath << "'.\n";
        }
        PlayerDebugTrace(oss.str());

        model = cache.Get(modelPath);
        if (!model)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Failed to load fairy model at '" + modelPath + "'.\n");
        }
        else
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Fairy model loaded successfully.\n");
        }
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

        HandleCarryInput(ctx);
        UpdateCarriedSweets();
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

void SceneSpace::Player::SetSweetsManager(SweetsSpace::SweetsManager* manager)
{
        if (m_sweetsManager == manager)
        {
                return;
        }

        if (!manager && m_carried)
        {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "SweetsManager cleared while carrying; forcing release.\n");
                m_carried.reset();
        }

        m_sweetsManager = manager;
}

bool SceneSpace::Player::IsCarryingSweets() const
{
        return static_cast<bool>(m_carried);
}

std::shared_ptr<SweetsSpace::BaseSweets> SceneSpace::Player::GetCarriedSweets() const
{
        return m_carried;
}

void SceneSpace::Player::ClearCarriedSweets()
{
        if (m_carried)
        {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Clearing carried sweets handle.\n");
        }
        m_carried.reset();
}

void SceneSpace::Player::UpdateCarriedSweets()
{
        if (!m_carried)
        {
                return;
        }

        auto& sweetsTransform = m_carried->GetTransform();
        sweetsTransform.position = ComputeCarryAnchor();
        sweetsTransform.rotation = m_transform.rotation;

        Physics::Physical& physical = m_carried->GetPhysical();
        physical.velocity = { 0.0f, 0.0f, 0.0f };
        physical.force = { 0.0f, 0.0f, 0.0f };
}

void SceneSpace::Player::HandleCarryInput(Core::UpdateContext& ctx)
{
        Core::Input& input = ctx.input;

        if (input.IsKeyTrigger(PLAYER_KEY_DROP) && m_carried)
        {
                DropCarriedSweets();
                return;
        }

        if (input.IsKeyTrigger(PLAYER_KEY_PICKUP) && !m_carried)
        {
                TryPickupNearest();
        }
}

void SceneSpace::Player::TryPickupNearest()
{
        if (!m_sweetsManager)
        {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Pickup skipped: SweetsManager missing.\n");
                return;
        }

        auto candidate = m_sweetsManager->FindNearestCarryable(m_transform.position, m_pickupRadius);
        if (!candidate)
        {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Pickup failed: no sweets within radius.\n");
                return;
        }

        if (!m_sweetsManager->TryMarkAsCarried(candidate))
        {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Pickup failed: manager rejected transition.\n");
                return;
        }

        auto& sweetsTransform = candidate->GetTransform();
        sweetsTransform.position = ComputeCarryAnchor();
        sweetsTransform.rotation = m_transform.rotation;

        auto& physical = candidate->GetPhysical();
        physical.velocity = { 0.0f, 0.0f, 0.0f };
        physical.force = { 0.0f, 0.0f, 0.0f };

        m_carried = candidate;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Pickup success tag='" << candidate->GetTag()
            << "' radius=" << m_pickupRadius << "\n";
        PlayerDebugTrace(oss.str());
}

void SceneSpace::Player::DropCarriedSweets()
{
        if (!m_carried)
        {
                return;
        }

        auto sweets = m_carried;
        auto& sweetsTransform = sweets->GetTransform();
        sweetsTransform.position = ComputeCarryAnchor();

        auto& sweetsPhysical = sweets->GetPhysical();
        sweetsPhysical.force = { 0.0f, 0.0f, 0.0f };
        sweetsPhysical.velocity = m_physical.velocity;
        sweetsPhysical.velocity.y = std::max(sweetsPhysical.velocity.y, m_dropUpwardBoost);

        if (m_sweetsManager)
        {
                if (!m_sweetsManager->ReleaseCarriedSweets(sweets))
                {
                        PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Drop warning: manager refused release.\n");
                }
        }
        else
        {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Drop warning: SweetsManager missing.\n");
        }

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Drop success tag='" << sweets->GetTag()
            << "' velocityY=" << sweetsPhysical.velocity.y << "\n";
        PlayerDebugTrace(oss.str());

        m_carried.reset();
}

float3 SceneSpace::Player::ComputeCarryAnchor() const
{
        return float3{
            m_transform.position.x + m_carryOffset.x,
            m_transform.position.y + m_carryOffset.y,
            m_transform.position.z + m_carryOffset.z };
}
