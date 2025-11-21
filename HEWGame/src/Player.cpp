#include "Player.h"
#include "Core/Input.h"
#include "ModelCache.h"
#include "Physical.h"
#include "Sweets.h"
#include "SweetsManager.h"
#include "SweetsPrototypeManifest.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>
#include <array>
#include <DirectXMath.h>
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
    constexpr float kDefaultPlayerMoveSpeed = 15.0f;

    std::shared_ptr<ModelDX> CreateProceduralFairyModel()
    {
        static std::weak_ptr<ModelDX> cached;
        if (auto reused = cached.lock())
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Reusing cached procedural fairy mesh.\n");
            return reused;
        }

        ID3D11Device* device = GetDevice();
        if (!device)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Cannot create procedural fairy: device not initialized.\n");
            return nullptr;
        }

        constexpr float halfWidth = 0.4f;
        constexpr float halfDepth = 0.12f;
        constexpr float halfHeight = 0.7f;

        const DirectX::XMFLOAT4 baseColor{ 0.45f, 0.65f, 1.0f, 1.0f };
        const DirectX::XMFLOAT4 wingHighlight{ 0.6f, 0.8f, 1.0f, 1.0f };

        std::vector<MeshDX::Vertex> vertices;
        vertices.reserve(24);

        auto appendQuad = [&](const std::array<DirectX::XMFLOAT3, 4>& positions, const DirectX::XMFLOAT3& normal,
            const DirectX::XMFLOAT4& colorFront, const DirectX::XMFLOAT4& colorBack)
        {
            const std::array<DirectX::XMFLOAT2, 4> uvs{ DirectX::XMFLOAT2(0.0f, 1.0f), DirectX::XMFLOAT2(1.0f, 1.0f),
                DirectX::XMFLOAT2(1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) };

            const auto addVertex = [&](const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& nrm, const DirectX::XMFLOAT2& uv,
                const DirectX::XMFLOAT4& col)
            {
                MeshDX::Vertex vertex{};
                vertex.pos = pos;
                vertex.normal = nrm;
                vertex.uv = uv;
                vertex.color = col;
                for (float& weight : vertex.weight)
                {
                    weight = 0.0f;
                }
                for (unsigned int& idx : vertex.index)
                {
                    idx = 0;
                }
                vertices.push_back(vertex);
            };

            const DirectX::XMFLOAT3 frontNormal = normal;
            const DirectX::XMFLOAT3 backNormal{ -normal.x, -normal.y, -normal.z };

            for (size_t i = 0; i < positions.size(); ++i)
            {
                addVertex(positions[i], frontNormal, uvs[i], colorFront);
            }
            for (size_t i = 0; i < positions.size(); ++i)
            {
                addVertex(positions[i], backNormal, uvs[i], colorBack);
            }
        };

        const std::array<DirectX::XMFLOAT3, 4> facingZ{ DirectX::XMFLOAT3(-halfWidth, -halfHeight, 0.0f),
            DirectX::XMFLOAT3(halfWidth, -halfHeight, 0.0f), DirectX::XMFLOAT3(halfWidth, halfHeight, 0.0f),
            DirectX::XMFLOAT3(-halfWidth, halfHeight, 0.0f) };
        appendQuad(facingZ, DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f), wingHighlight, baseColor);

        const std::array<DirectX::XMFLOAT3, 4> facingX{ DirectX::XMFLOAT3(0.0f, -halfHeight, -halfDepth),
            DirectX::XMFLOAT3(0.0f, -halfHeight, halfDepth), DirectX::XMFLOAT3(0.0f, halfHeight, halfDepth),
            DirectX::XMFLOAT3(0.0f, halfHeight, -halfDepth) };
        appendQuad(facingX, DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f), baseColor, wingHighlight);

        std::vector<unsigned long> indices;
        indices.reserve(36);
        for (size_t quad = 0; quad < 2; ++quad)
        {
            const unsigned long base = static_cast<unsigned long>(quad * 8);
            const std::array<unsigned long, 6> front{ base, base + 1, base + 2, base, base + 2, base + 3 };
            const std::array<unsigned long, 6> back{ base + 4, base + 6, base + 5, base + 4, base + 7, base + 6 };
            indices.insert(indices.end(), front.begin(), front.end());
            indices.insert(indices.end(), back.begin(), back.end());
        }

        MeshDX::Description desc{};
        desc.pVtx = vertices.data();
        desc.vtxSize = sizeof(MeshDX::Vertex);
        desc.vtxCount = static_cast<UINT>(vertices.size());
        desc.pIdx = indices.data();
        desc.idxSize = sizeof(unsigned long);
        desc.idxCount = static_cast<UINT>(indices.size());
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        auto mesh = std::make_shared<MeshDX>(desc, 0);
        if (!mesh || !mesh->GetVertecBuffer() || !mesh->GetIndexBuffer())
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Procedural fairy mesh buffers failed to initialize.\n");
            return nullptr;
        }

        const DirectX::XMFLOAT4 diffuse{ 0.5f, 0.75f, 1.0f, 1.0f };
        const DirectX::XMFLOAT4 ambient{ 0.2f, 0.35f, 0.55f, 1.0f };
        const DirectX::XMFLOAT4 specular{ 0.1f, 0.1f, 0.2f, 8.0f };
        auto material = std::make_shared<MaterialDX>(diffuse, ambient, specular, nullptr);

        std::vector<std::shared_ptr<MeshDX>> meshes;
        meshes.push_back(std::move(mesh));

        std::vector<std::shared_ptr<MaterialDX>> materials;
        materials.push_back(std::move(material));

        auto model = std::make_shared<ModelDX>(std::move(meshes), std::move(materials));
        cached = model;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Procedural fairy mesh created (verts=" << vertices.size()
            << ", indices=" << indices.size() << ").\n";
        PlayerDebugTrace(oss.str());

        return model;
    }
}

SceneSpace::Player::Player(std::string a_tag,
        float3 pos,
        float3 rot,
        float3 scl,
        std::shared_ptr<Collision::ICollider> collide,
        Collision::CollisionLayer layer,
        float3 velocity,
        float3 force,
        float weight,
        float damping,
        Core::UpdatePhase phase,
        int priority,
        bool kinematic)
        : PhysicalEntity(a_tag, pos, rot, scl, collide, layer, velocity, force, kinematic ? 0.0f : weight, damping, phase, priority)
        , model(nullptr)
        , m_sweetsManager(nullptr)
        , m_carried()
        , m_pickupRadius(kDefaultPickupRadius)
        , m_carryOffset(float3{ 0.0f, kCarryVerticalOffset, kCarryForwardOffset })
        , m_dropUpwardBoost(4.0f)
        , m_isKinematic(kinematic)
        , m_moveSpeed(kDefaultPlayerMoveSpeed)
        , m_hasMinimumY(false)
        , m_minimumY(0.0f)
        , m_missingModelWarned(false)
        , m_hasScreenBounds(false)
        , m_minScreenX(0.0f)
        , m_maxScreenX(0.0f)
        , m_hasMaximumY(false)
        , m_maximumY(0.0f)
{
        Render::ModelCache& cache = Render::ModelCache::Instance();
        const auto& manifest = SweetsSpace::SweetsPrototypeManifest::Instance();
        const auto* descriptor = manifest.Find(SweetsSpace::SweetsType::Fairy);
        const std::string modelPath = descriptor ? descriptor->modelPath : std::string(kFairyFallbackPath);

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

        bool usingProcedural = false;
        model = CreateProceduralFairyModel();
        if (model)
        {
                usingProcedural = true;
                std::ostringstream success;
                success << kPlayerLogPrefix << "Using procedural fairy model; manifest asset reserved for fallback='" << modelPath
                        << "'.\n";
                PlayerDebugTrace(success.str());
        }

        if (!model)
        {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Procedural fairy model unavailable; attempting to load asset file.\n");
                model = cache.Get(modelPath);
        }

        if (model)
        {
                std::ostringstream success;
                if (usingProcedural)
                {
                        success << kPlayerLogPrefix << "Fairy model ready from procedural mesh with fallback asset='" << modelPath
                                << "'.\n";
                }
                else
                {
                        success << kPlayerLogPrefix << "Fairy model ready from "
                                << (modelPath == kFairyFallbackPath ? "fallback" : "asset")
                                << " path '" << modelPath << "'.\n";
                }
                PlayerDebugTrace(success.str());
        }
        else
        {
                std::ostringstream failure;
                const bool isAlreadyFallback = (modelPath == kFairyFallbackPath);

                failure << kPlayerLogPrefix << "Failed to load fairy model at '" << modelPath << "'.";
                if (!isAlreadyFallback)
                {
                        failure << " Attempting fallback '" << kFairyFallbackPath << "'.\n";
                }
                else
                {
                        failure << " Fallback path already in use.\n";
                }
                PlayerDebugTrace(failure.str());

                if (!isAlreadyFallback)
                {
                        model = cache.Get(kFairyFallbackPath);
                        if (model)
                        {
                                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Fallback fairy model loaded successfully.\n");
                        }
                }

                if (!model)
                {
                        PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Player model unavailable; rendering will be skipped.\n");
                        m_missingModelWarned = true;
                }
        }
}

bool SceneSpace::Player::IsKinematic() const
{
        return m_isKinematic;
}

void SceneSpace::Player::OnUpdate(Core::UpdateContext& ctx)
{
        Core::Input& input = ctx.input;

        float horizontal = 0.0f;
        const bool moveLeft = input.IsKeyPress(PLAYER_KEY_MOVE_LEFT);
        const bool moveRight = input.IsKeyPress(PLAYER_KEY_MOVE_RIGHT);

        if (moveLeft && !moveRight)
        {
                horizontal = -1.0f;
        }
        else if (moveRight && !moveLeft)
        {
                horizontal = 1.0f;
        }

        float vertical = 0.0f;
        const bool moveUp = input.IsKeyPress(PLAYER_KEY_MOVE_UP);
        const bool moveDown = input.IsKeyPress(PLAYER_KEY_MOVE_DOWN);

        if (moveUp && !moveDown)
        {
                vertical = 1.0f;
        }
        else if (moveDown && !moveUp)
        {
                vertical = -1.0f;
        }

        const bool canMove = (ctx.dt > 0.0f && m_moveSpeed > 0.0f);
        if (canMove && (horizontal != 0.0f || vertical != 0.0f))
        {
                float3 translation{ 0.0f, 0.0f, 0.0f };
                translation.x = horizontal * m_moveSpeed * ctx.dt;
                translation.y = vertical * m_moveSpeed * ctx.dt;
                ApplyTranslation(translation);
        }

        ClampVerticalPosition();
        ClampScreenBounds();

        HandleCarryInput(ctx);
        UpdateCarriedSweets();
}

void SceneSpace::Player::Draw(ModelRender& render, CameraSpace::ICamera& camera)
{
        if (!model)
        {
                if (!m_missingModelWarned)
                {
                        PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Draw skipped: player model missing.\n");
                        m_missingModelWarned = true;
                }
                return;
        }

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

void SceneSpace::Player::SetMoveSpeed(float speed)
{
        if (!std::isfinite(speed))
        {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Ignored non-finite move speed assignment.\n");
                return;
        }

        const float sanitized = std::max(0.0f, speed);
        if (sanitized != speed)
        {
                std::ostringstream oss;
                oss << kPlayerLogPrefix << "Move speed sanitized from " << speed
                    << " to " << sanitized << " (negative disallowed).\n";
                PlayerDebugTrace(oss.str());
        }

        if (std::fabs(m_moveSpeed - sanitized) < std::numeric_limits<float>::epsilon())
        {
                return;
        }

        m_moveSpeed = sanitized;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Move speed set to " << m_moveSpeed << " units/sec.\n";
        PlayerDebugTrace(oss.str());
}

float SceneSpace::Player::GetMoveSpeed() const
{
        return m_moveSpeed;
}

void SceneSpace::Player::SetMinimumY(float minY)
{
        if (!std::isfinite(minY))
        {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Ignored non-finite minimum Y assignment.\n");
                return;
        }

        if (m_hasMinimumY && std::fabs(m_minimumY - minY) < std::numeric_limits<float>::epsilon())
        {
                return;
        }

        m_hasMinimumY = true;
        m_minimumY = minY;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Minimum Y set to " << m_minimumY << " units.\n";
        PlayerDebugTrace(oss.str());

        ClampVerticalPosition();
}

float SceneSpace::Player::GetMinimumY() const
{
        return m_minimumY;
}

bool SceneSpace::Player::HasMinimumY() const
{
        return m_hasMinimumY;
}

void SceneSpace::Player::SetScreenBounds(float minX, float maxX, std::optional<float> maxY)
{
        if (!std::isfinite(minX) || !std::isfinite(maxX))
        {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Ignored non-finite screen bounds.\n");
                return;
        }

        const float sanitizedMinX = std::min(minX, maxX);
        const float sanitizedMaxX = std::max(minX, maxX);
        const bool hadBounds = m_hasScreenBounds;
        const bool boundsChanged = !hadBounds
            || std::fabs(m_minScreenX - sanitizedMinX) > std::numeric_limits<float>::epsilon()
            || std::fabs(m_maxScreenX - sanitizedMaxX) > std::numeric_limits<float>::epsilon()
            || (m_hasMaximumY != maxY.has_value())
            || (maxY.has_value() && std::fabs(m_maximumY - *maxY) > std::numeric_limits<float>::epsilon());

        m_hasScreenBounds = true;
        m_minScreenX = sanitizedMinX;
        m_maxScreenX = sanitizedMaxX;
        m_hasMaximumY = maxY.has_value() && std::isfinite(*maxY);
        if (m_hasMaximumY)
        {
                m_maximumY = *maxY;
        }

        if (!boundsChanged)
        {
                return;
        }

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Screen bounds set minX=" << m_minScreenX
            << " maxX=" << m_maxScreenX;
        if (m_hasMaximumY)
        {
                oss << " maxY=" << m_maximumY;
        }
        oss << "\n";
        PlayerDebugTrace(oss.str());
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

        m_carried->SyncColliderBounds();

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
        candidate->SyncColliderBounds();

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
        sweets->SyncColliderBounds();

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

void SceneSpace::Player::ClampVerticalPosition()
{
        if (!m_hasMinimumY)
        {
                return;
        }

        auto& position = m_transform.position;
        if (position.y >= m_minimumY)
        {
                return;
        }

        const float previousY = position.y;
        position.y = m_minimumY;
        SyncColliderBounds();

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Clamped Y from " << previousY << " to minimum "
            << m_minimumY << " (platform boundary).\n";
        PlayerDebugTrace(oss.str());
}

void SceneSpace::Player::ClampScreenBounds()
{
        if (!m_hasScreenBounds)
        {
                return;
        }

        auto& position = m_transform.position;
        const float originalX = position.x;
        const float originalY = position.y;

        const float clampedX = std::clamp(position.x, m_minScreenX, m_maxScreenX);
        float clampedY = position.y;
        if (m_hasMaximumY)
        {
                clampedY = std::min(clampedY, m_maximumY);
        }

        if (clampedX == originalX && clampedY == originalY)
        {
                return;
        }

        position.x = clampedX;
        position.y = clampedY;
        SyncColliderBounds();

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Screen clamp applied prevX=" << originalX
            << " prevY=" << originalY
            << " newX=" << position.x
            << " newY=" << position.y
            << " boundsX=[" << m_minScreenX << ',' << m_maxScreenX << ']';
        if (m_hasMaximumY)
        {
                oss << " maxY=" << m_maximumY;
        }
        oss << "\n";
        PlayerDebugTrace(oss.str());
}
