#include "Entities/Player.h"

#include "AABB.h"
#include "Core/Input.h"
#include "BoxColliderComponent.h"
#include "ModelCache.h"
#include "Systems/PhysicsMachine.h"
#include "Sweets/BaseSweets.h"
#include "SweetsManager.h"
#include "SweetsPrototypeManifest.h"
#include "Entities/PhysicalEntity.h"


#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <utility>
#include <vector>
#include <DirectXMath.h>

#include <box2d/box2d.h>
#include <box2d/math_functions.h>

#if defined(_WIN32)
#    ifndef NOMINMAX
#        define NOMINMAX
#    endif
#    include <windows.h>
#endif

namespace
{
#if defined(_WIN32) && defined(_DEBUG)
    inline void PlayerDebugTrace(const std::string& message)
    {
        ::OutputDebugStringA(message.c_str());
    }
#else
    inline void PlayerDebugTrace(const std::string& message)
    {
        (void)message;
    }
#endif

    constexpr const char* kPlayerLogPrefix = "[Player] ";
    constexpr const char* kFairyFallbackPath = "Assets/Model/SweetsPrototype/FairyPlaceholder.obj";

    constexpr float kCarryVerticalOffset = 1.0f;
    constexpr float kCarryForwardOffset = 0.5f;

    constexpr int kDefaultMaxCarry = 3;

    std::optional<AABB> ComputeBounds(const std::vector<MeshDX::Vertex>& vertices)
    {
        if (vertices.empty())
        {
            return std::nullopt;
        }

        float3 min
        {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
        };
        float3 max
        {
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
        };

        for (const auto& vertex : vertices)
        {
            min.x = std::min(min.x, vertex.pos.x);
            min.y = std::min(min.y, vertex.pos.y);
            min.z = std::min(min.z, vertex.pos.z);

            max.x = std::max(max.x, vertex.pos.x);
            max.y = std::max(max.y, vertex.pos.y);
            max.z = std::max(max.z, vertex.pos.z);
        }

        return AABB(max, min);
    }

    std::shared_ptr<Render::Model> CreateProceduralFairyModel()
    {
        static std::weak_ptr<Render::Model> cached;
        if (auto reused = cached.lock())
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Reusing cached procedural fairy.\n");
            return reused;
        }

        ID3D11Device* device = GetDevice();
        if (!device)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Cannot create procedural fairy: device missing.\n");
            return nullptr;
        }

        constexpr float halfWidth = 0.4f;
        constexpr float halfDepth = 0.12f;
        constexpr float halfHeight = 0.7f;

        const DirectX::XMFLOAT4 baseColor{ 0.45f, 0.65f, 1.0f, 1.0f };
        const DirectX::XMFLOAT4 wingColor{ 0.6f, 0.8f, 1.0f, 1.0f };

        std::vector<MeshDX::Vertex> vertices;
        vertices.reserve(24);

        auto appendQuad = [&](auto positions, DirectX::XMFLOAT3 normal,
            const DirectX::XMFLOAT4& colFront,
            const DirectX::XMFLOAT4& colBack)
            {
                const std::array<DirectX::XMFLOAT2, 4> uvs{
                    DirectX::XMFLOAT2(0,1), DirectX::XMFLOAT2(1,1),
                    DirectX::XMFLOAT2(1,0), DirectX::XMFLOAT2(0,0)
                };

                auto addV = [&](auto pos, auto nrm, auto uv, auto col)
                    {
                        MeshDX::Vertex v{};
                        v.pos = pos;
                        v.normal = nrm;
                        v.uv = uv;
                        v.color = col;
                        for (float& w : v.weight) w = 0;
                        for (unsigned& i : v.index) i = 0;
                        vertices.push_back(v);
                    };

                DirectX::XMFLOAT3 n1 = normal;
                DirectX::XMFLOAT3 n2{ -normal.x, -normal.y, -normal.z };

                for (int i = 0; i < 4; i++) addV(positions[i], n1, uvs[i], colFront);
                for (int i = 0; i < 4; i++) addV(positions[i], n2, uvs[i], colBack);
            };

        const std::array<DirectX::XMFLOAT3, 4> quadZ{
            DirectX::XMFLOAT3(-halfWidth,-halfHeight,0),
            DirectX::XMFLOAT3(halfWidth,-halfHeight,0),
            DirectX::XMFLOAT3(halfWidth, halfHeight,0),
            DirectX::XMFLOAT3(-halfWidth, halfHeight,0)
        };
        appendQuad(quadZ, DirectX::XMFLOAT3(0, 0, 1), wingColor, baseColor);

        const std::array<DirectX::XMFLOAT3, 4> quadX{
            DirectX::XMFLOAT3(0,-halfHeight,-halfDepth),
            DirectX::XMFLOAT3(0,-halfHeight, halfDepth),
            DirectX::XMFLOAT3(0, halfHeight, halfDepth),
            DirectX::XMFLOAT3(0, halfHeight,-halfDepth)
        };
        appendQuad(quadX, DirectX::XMFLOAT3(1, 0, 0), baseColor, wingColor);

        std::vector<unsigned long> indices;
        indices.reserve(36);

        for (int q = 0; q < 2; q++)
        {
            unsigned long base = q * 8;
            unsigned long f[6] = { base, base + 1, base + 2, base, base + 2, base + 3 };
            unsigned long b[6] = { base + 4, base + 6, base + 5, base + 4, base + 7, base + 6 };
            indices.insert(indices.end(), f, f + 6);
            indices.insert(indices.end(), b, b + 6);
        }

        MeshDX::Description d{};
        d.pVtx = vertices.data();
        d.vtxSize = sizeof(MeshDX::Vertex);
        d.vtxCount = (UINT)vertices.size();
        d.pIdx = indices.data();
        d.idxSize = sizeof(unsigned long);
        d.idxCount = (UINT)indices.size();
        d.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

        auto mesh = std::make_shared<MeshDX>(d, 0);
        if (!mesh->GetVertexBuffer().IsValid() || !mesh->HasIndexBuffer())
            return nullptr;

        auto mat = std::make_shared<MaterialDX>(
            DirectX::XMFLOAT4(0.5f, 0.75f, 1, 1),
            DirectX::XMFLOAT4(0.2f, 0.35f, 0.55f, 1),
            DirectX::XMFLOAT4(0.1f, 0.1f, 0.2f, 8),
            nullptr);

        const std::optional<AABB> bounds = ComputeBounds(vertices);

        auto model = std::make_shared<ModelDX>(
            std::vector<std::shared_ptr<MeshDX>>{mesh},
            std::vector<std::shared_ptr<MaterialDX>>{mat});

        if (bounds)
        {
            model->SetLocalBounds(*bounds);
        }

        cached = model;
        return model;
    }

    Physics::Box2DShapeConfig BuildDefaultPlayerShapeConfig(const Transform& transform, bool isKinematic)
    {
        Physics::Box2DShapeConfig config{};
        config.type = Physics::Box2DShapeConfig::Type::Polygon;
        config.bodyType = isKinematic ? b2_kinematicBody : b2_dynamicBody;
        config.fixedRotation = true;
        config.allowRotation = false;
        config.angularDamping = 5.0f;
        config.density = 0.5f;
        config.friction = 0.35f;
        config.restitution = 0.05f;

        const float halfWidth = 0.35f;
        const float halfHeight = 0.5f;

        config.vertices = {
            b2Vec2{ -halfWidth, -halfHeight },
            b2Vec2{ halfWidth, -halfHeight },
            b2Vec2{ halfWidth, halfHeight },
            b2Vec2{ -halfWidth, halfHeight },
        };

        return config;
    }
}

namespace Entities
{
    Player::Player(const Transform& transform, std::shared_ptr<Render::Model> model, Systems::PhysicsMachine* physicsMachine, SweetsSpace::SweetsManager* sweetsManager, Collision::CollisionLayer collisionLayer, bool kinematic, Core::UpdatePhase phase, int priority, std::string tag)
        : PhysicalEntity(transform, std::nullopt, collisionLayer, std::move(model), physicsMachine, phase, priority, std::move(tag))
        , m_fairyModel(nullptr)
        , m_sweetsManager(sweetsManager)
        , m_carrieds()
        , m_maxCarry(kDefaultMaxCarry)
        , m_pickupHalfWidth(0.0f)
        , m_pickupHalfHeight(0.0f)
        , m_pickupForwardOffset(0.0f)
        , m_carryOffset(float3{ 0, kCarryVerticalOffset, kCarryForwardOffset })
        , m_dropUpwardBoost(4.0f)
        , m_isKinematic(kinematic)
        , m_baseHorizontalMoveSpeed(0.0f)
        , m_baseVerticalMoveSpeed(0.0f)
        , m_speedMultiplier(1.0f)
        , m_hasMinimumY(false)
        , m_minimumY(0.0f)
        , m_missingModelWarned(false)
        , m_hasScreenBounds(false)
        , m_minScreenX(0.0f)
        , m_maxScreenX(0.0f)
        , m_hasMaximumY(false)
        , m_maximumY(0.0f)
    {
        if (model)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Using provided player model.\n");
        }
        else
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "No player model supplied; resolving fairy prototype.\n");
        }

        Render::ModelCache& cache = Render::ModelCache::Instance();
        const auto& manifest = SweetsSpace::SweetsPrototypeManifest::Instance();

        const auto* descriptor = manifest.Find(SweetsSpace::SweetsType::Fairy);
        std::string modelPath = descriptor ? descriptor->modelPath : std::string{};
        if (!descriptor)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Fairy descriptor missing; using placeholder fallback.\n");
            modelPath = kFairyFallbackPath;
        }
        else if (modelPath.empty())
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Fairy descriptor path empty; using placeholder fallback.\n");
            modelPath = kFairyFallbackPath;
        }
        else
        {
            std::ostringstream oss;
            oss << kPlayerLogPrefix << "Resolved fairy descriptor key='" << descriptor->key
                << "' path='" << descriptor->modelPath << "'.\n";
            PlayerDebugTrace(oss.str());
        }

        m_fairyModel = CreateProceduralFairyModel();
        if (!m_fairyModel)
        {
            m_fairyModel = cache.Get(modelPath);
            if (!m_fairyModel && modelPath != kFairyFallbackPath)
            {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Primary fairy model load failed; trying placeholder.\n");
                m_fairyModel = cache.Get(kFairyFallbackPath);
            }
        }

        if (!m_fairyModel)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "No fairy model available after fallbacks.\n");
            m_missingModelWarned = true;
        }

        if (!m_model)
        {
            m_model = m_fairyModel;
        }

        const auto shapeConfig = BuildDefaultPlayerShapeConfig(m_transform, m_isKinematic);
        Physics::BoxColliderComponent colliderComponent;
        colliderComponent.shapeDef.density = shapeConfig.density;
        colliderComponent.shapeDef.material.friction = shapeConfig.friction;
        colliderComponent.shapeDef.material.restitution = shapeConfig.restitution;
        colliderComponent.AddPolygon(shapeConfig.vertices);

        SetBoxColliderComponent(colliderComponent);
        SetBox2DShapeConfig(shapeConfig);
    }

    void Player::OnUpdate(Core::UpdateContext& ctx)
    {
        SyncTransformFromBox2DBody();

        if (!HasBox2DBody())
            return;

        Core::Input& input = ctx.input;

        const float horizontal = input.GetAxis(Core::ActionId::MoveX);
        const float vertical = input.GetAxis(Core::ActionId::RotateYaw);

        const float effectiveHorizontalSpeed = m_baseHorizontalMoveSpeed * m_speedMultiplier;
        const float effectiveVerticalSpeed = m_baseVerticalMoveSpeed * m_speedMultiplier;

        if (ctx.dt > 0.0f && (effectiveHorizontalSpeed > 0.0f || effectiveVerticalSpeed > 0.0f))
        {
            if (horizontal != 0.0f || vertical != 0.0f)
            {
                if (HasBox2DBody())
                {
                    const float3 desired{
                        horizontal * effectiveHorizontalSpeed,
                        vertical * effectiveVerticalSpeed,
                        0.0f
                    };

                    SetLinearVelocity(desired);
                }
            }
            else if (HasBox2DBody())
            {
                SetLinearVelocity(float3{ 0,0,0 });
            }
        }

        ClampVerticalPosition();
        ClampScreenBounds();
        HandleCarryInput(ctx);
        UpdateCarriedSweets();

        SyncTransformFromBox2DBody();
    }

    void Player::Draw(Render::ModelRenderer& renderer, const ::CameraSpace::ICamera& camera)
    {
        if (!m_model)
        {
            if (!m_missingModelWarned)
            {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Draw skipped: player model missing.\n");
                m_missingModelWarned = true;
            }
            return;
        }

        SyncTransformFromBox2DBody();
        renderer.Draw(*m_model, m_transform, &camera);
    }

    void Player::SetSweetsManager(SweetsSpace::SweetsManager* manager)
    {
        if (m_sweetsManager == manager)
        {
            return;
        }

        if (!manager && !m_carrieds.empty())
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "SweetsManager cleared while carrying; forcing release.\n");
            m_carrieds.clear();
        }

        m_sweetsManager = manager;
    }

    bool Player::IsCarryingSweets() const
    {
        return !m_carrieds.empty();
    }

    const std::vector<std::shared_ptr<SweetsSpace::BaseSweets>>& Player::GetCarriedSweets() const
    {
        return m_carrieds;
    }

    void Player::ClearCarriedSweets()
    {
        if (m_carrieds.empty())
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Clearing carried sweets handle.\n");
        }
        m_carrieds.clear();
    }

    bool Player::IsKinematic() const
    {
        return m_isKinematic;
    }

    void Player::SetMoveSpeed(float speed)
    {
        SetMoveSpeeds(speed, speed);
    }

    void Player::SetMoveSpeeds(float horizontal, float vertical)
    {
        SetBaseMoveSpeed(horizontal, vertical);
        SetSpeedMultiplier(1.0f);
    }

    void Player::SetBaseMoveSpeed(float horizontal, float vertical)
    {
        if (!std::isfinite(horizontal) || !std::isfinite(vertical))
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Ignored non-finite move speed assignment.\n");
            return;
        }

        const float sanitizedHorizontal = std::max(0.0f, horizontal);
        const float sanitizedVertical = std::max(0.0f, vertical);

        if (sanitizedHorizontal != horizontal || sanitizedVertical != vertical)
        {
            std::ostringstream oss;
            oss << kPlayerLogPrefix << "Move speeds sanitized from (" << horizontal << ',' << vertical
                << ") to (" << sanitizedHorizontal << ',' << sanitizedVertical << ") (negative disallowed).\n";
            PlayerDebugTrace(oss.str());
        }

        const bool unchanged = std::fabs(m_baseHorizontalMoveSpeed - sanitizedHorizontal) < std::numeric_limits<float>::epsilon()
            && std::fabs(m_baseVerticalMoveSpeed - sanitizedVertical) < std::numeric_limits<float>::epsilon();
        if (unchanged)
        {
            return;
        }

        m_baseHorizontalMoveSpeed = sanitizedHorizontal;
        m_baseVerticalMoveSpeed = sanitizedVertical;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Base move speeds set to (" << m_baseHorizontalMoveSpeed
            << ", " << m_baseVerticalMoveSpeed << ") units/sec.\n";
        PlayerDebugTrace(oss.str());
    }

    void Player::SetSpeedMultiplier(float multiplier)
    {
        if (!std::isfinite(multiplier))
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Ignored non-finite speed multiplier assignment.\n");
            return;
        }

        const float sanitizedMultiplier = std::max(0.0f, multiplier);
        const bool unchanged = std::fabs(m_speedMultiplier - sanitizedMultiplier) < std::numeric_limits<float>::epsilon();
        if (unchanged)
        {
            return;
        }

        m_speedMultiplier = sanitizedMultiplier;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Speed multiplier set to " << m_speedMultiplier << "x.\n";
        PlayerDebugTrace(oss.str());
    }

    float Player::GetMoveSpeed() const
    {
        return GetHorizontalMoveSpeed();
    }

    float Player::GetHorizontalMoveSpeed() const
    {
        return m_baseHorizontalMoveSpeed * m_speedMultiplier;
    }

    float Player::GetVerticalMoveSpeed() const
    {
        return m_baseVerticalMoveSpeed * m_speedMultiplier;
    }

    void Player::SetMinimumY(float minY)
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

    float Player::GetMinimumY() const
    {
        return m_minimumY;
    }

    bool Player::HasMinimumY() const
    {
        return m_hasMinimumY;
    }

    void Player::SetScreenBounds(float minX, float maxX, std::optional<float> maxY)
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

    void Player::SetPickupBox(float width, float height)
    {
        SetPickupArea(width, height, m_pickupForwardOffset);
    }

    void Player::SetPickupArea(float width, float height, float forwardOffset)
    {
        if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(forwardOffset))
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Ignored non-finite pickup box assignment.\n");
            return;
        }

        const float sanitizedWidth = std::max(0.0f, width);
        const float sanitizedHeight = std::max(0.0f, height);
        const float sanitizedForwardOffset = std::max(0.0f, forwardOffset);
        const float halfWidth = sanitizedWidth * 0.5f;
        const float halfHeight = sanitizedHeight * 0.5f;

        const bool unchanged = std::fabs(m_pickupHalfWidth - halfWidth) < std::numeric_limits<float>::epsilon()
            && std::fabs(m_pickupHalfHeight - halfHeight) < std::numeric_limits<float>::epsilon()
            && std::fabs(m_pickupForwardOffset - sanitizedForwardOffset) < std::numeric_limits<float>::epsilon();
        if (unchanged)
        {
            return;
        }

        m_pickupHalfWidth = halfWidth;
        m_pickupHalfHeight = halfHeight;
        m_pickupForwardOffset = sanitizedForwardOffset;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Pickup box set to width=" << sanitizedWidth
            << " height=" << sanitizedHeight << " (half=" << m_pickupHalfWidth
            << ',' << m_pickupHalfHeight << ") forwardOffset=" << m_pickupForwardOffset << ".\n";
        PlayerDebugTrace(oss.str());
    }

    float3 Player::ComputeCarryAnchor() const
    {
        if (HasBox2DBody())
        {
            const float3 position = ToWorldPosition(b2Body_GetPosition(GetBox2DBodyId()));
            return float3{ position.x + m_carryOffset.x, position.y + m_carryOffset.y, position.z + m_carryOffset.z };
        }

        return float3{
            m_transform.position.x + m_carryOffset.x,
            m_transform.position.y + m_carryOffset.y,
            m_transform.position.z + m_carryOffset.z };
    }

    void Player::UpdateCarriedSweets()
    {
        if (m_carrieds.empty())
        {
            return;
        }

        SyncTransformFromBox2DBody();

        for (auto& it : m_carrieds)
        {
            const Transform current = it ? it->GetTransform() : Transform{};
            Transform updated = current;
            updated.position = ComputeCarryAnchor();
            updated.rotation = m_transform.rotation;

            it->SetTransform(updated);
            it->SetLinearVelocity({ 0.0f, 0.0f, 0.0f });
        }

    }

    void Player::HandleCarryInput(Core::UpdateContext& ctx)
    {
        Core::Input& input = ctx.input;

        if (input.IsTrigger(Core::ActionId::Drop) && !m_carrieds.empty())
        {
            DropCarriedSweets();
            return;
        }

        if (input.IsTrigger(Core::ActionId::Confirm) && m_carrieds.size() < m_maxCarry)
        {
            TryPickupNearest();
        }
    }

    void Player::TryPickupNearest()
    {
        if (!m_sweetsManager)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Pickup skipped: SweetsManager missing.\n");
            return;
        }

        if (!(m_pickupHalfWidth > 0.0f) || !(m_pickupHalfHeight > 0.0f))
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Pickup skipped: pickup box not configured.\n");
            return;
        }

        if (!m_colliderFootprint)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Pickup skipped: collider footprint missing.\n");
            return;
        }

        SyncTransformFromBox2DBody();

        const float3 colliderCenter
        {
            m_transform.position.x + m_colliderFootprint->centerOffset.x,
            m_transform.position.y + m_colliderFootprint->centerOffset.y,
            m_transform.position.z + m_colliderFootprint->centerOffset.z,
        };

        const float halfDepth = m_colliderFootprint->halfExtents.z + 0.05f;

        const float3 pickupCenter{ colliderCenter.x + m_pickupForwardOffset, colliderCenter.y, colliderCenter.z };

        AABB pickupBounds{};
        pickupBounds.min.x = pickupCenter.x - m_pickupHalfWidth;
        pickupBounds.max.x = pickupCenter.x + m_pickupHalfWidth;
        pickupBounds.min.y = pickupCenter.y - m_pickupHalfHeight;
        pickupBounds.max.y = pickupCenter.y + m_pickupHalfHeight;
        pickupBounds.min.z = pickupCenter.z - halfDepth;
        pickupBounds.max.z = pickupCenter.z + halfDepth;

        std::shared_ptr<SweetsSpace::ActiveSweets> candidate;
        float bestDistanceSq = std::numeric_limits<float>::max();

        const auto considerState = [&](SweetsSpace::SweetsState state)
            {
                const auto& sweetsList = m_sweetsManager->GetByState(state);
                for (const auto& sweets : sweetsList)
                {
                    if (!sweets || sweets->IsPendingDestroy())
                    {
                        continue;
                    }

                    if (std::find(m_carrieds.begin(), m_carrieds.end(), sweets) != m_carrieds.end())
                    {
                        continue;
                    }

                    sweets->SyncColliderBounds();
                    const AABB sweetsBounds = sweets->GetAABB();
                    if (!sweetsBounds.Intersects(pickupBounds))
                    {
                        continue;
                    }

                    const float sweetsCenterX = (sweetsBounds.min.x + sweetsBounds.max.x) * 0.5f;
                    const float sweetsCenterY = (sweetsBounds.min.y + sweetsBounds.max.y) * 0.5f;
                    const float sweetsCenterZ = (sweetsBounds.min.z + sweetsBounds.max.z) * 0.5f;

                    const float dx = sweetsCenterX - pickupCenter.x;
                    const float dy = sweetsCenterY - pickupCenter.y;
                    const float dz = sweetsCenterZ - pickupCenter.z;
                    const float distanceSq = (dx * dx) + (dy * dy) + (dz * dz);

                    if (distanceSq <= bestDistanceSq)
                    {
                        bestDistanceSq = distanceSq;
                        candidate = sweets;
                    }
                }
            };

        considerState(SweetsSpace::SweetsState::Ground);
        considerState(SweetsSpace::SweetsState::Active);

        if (!candidate)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Pickup failed: no sweets within pickup box.\n");
            return;
        }

        if (!m_sweetsManager->TryMarkAsCarried(candidate))
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Pickup failed: manager rejected transition.\n");
            return;
        }

        candidate->SyncColliderBounds();
        const Transform current = candidate->GetTransform();
        Transform updated = current;
        updated.position = ComputeCarryAnchor();
        updated.rotation = m_transform.rotation;
        candidate->SetTransformViaPhysics(updated.position, updated.rotation);

        candidate->SetLinearVelocity({ 0.0f, 0.0f, 0.0f });

        m_carrieds.push_back(candidate);

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Pickup success tag='" << candidate->GetTag()
            << "' boxHalf=(" << m_pickupHalfWidth << ',' << m_pickupHalfHeight
            << ") forwardOffset=" << m_pickupForwardOffset << "\n";
        PlayerDebugTrace(oss.str());
    }

    void Player::SetColliderFootprint(const float3& halfExtents, const float3& centerOffset)
    {
        m_colliderFootprint = ColliderFootprint{ halfExtents, centerOffset };
    }

    void Player::DropCarriedSweets()
    {
        if (m_carrieds.empty())
        {
            return;
        };

        auto sweets = m_carrieds.back();
        sweets->SetPositionViaPhysics(ComputeCarryAnchor());

        const float3 bodyVelocity = GetLinearVelocity();
        float3 releaseVelocity = bodyVelocity;
        releaseVelocity.y = std::max(releaseVelocity.y, m_dropUpwardBoost);
        sweets->SetLinearVelocity(releaseVelocity);

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
            << "' velocityY=" << sweets->GetLinearVelocity().y << "\n";
        PlayerDebugTrace(oss.str());

        m_carrieds.pop_back();
    }

    void Player::ClampVerticalPosition()
    {
        if (!m_hasMinimumY)
        {
            return;
        }

        if (!HasBox2DBody())
        {
            return;
        }

        const b2BodyId bodyId = GetBox2DBodyId();
        const float3 bodyPosition = ToWorldPosition(b2Body_GetPosition(bodyId));
        if (bodyPosition.y >= m_minimumY)
        {
            return;
        }

        const float previousY = bodyPosition.y;
        const float3 bodyVelocity = GetLinearVelocity();
        b2Body_SetTransform(
            bodyId,
            ToBox2DPosition(float3{ bodyPosition.x, m_minimumY, 0.0f }),
            b2Body_GetRotation(bodyId));
        float3 clampedVelocity = bodyVelocity;
        clampedVelocity.y = 0.0f;
        SetLinearVelocity(clampedVelocity);
        SyncTransformFromBox2DBody();

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Clamped Y from " << previousY << " to minimum "
            << m_minimumY << " (platform boundary).\n";
        PlayerDebugTrace(oss.str());
    }

    void Player::ClampScreenBounds()
    {
        if (!m_hasScreenBounds)
        {
            return;
        }

        if (!HasBox2DBody())
        {
            return;
        }

        const b2BodyId bodyId = GetBox2DBodyId();
        const float3 position = ToWorldPosition(b2Body_GetPosition(bodyId));
        const float originalX = position.x;
        const float originalY = position.y;

        const float clampedX = std::clamp(position.x, m_minScreenX, m_maxScreenX);
        float clampedY = position.y;
        if (m_hasMaximumY)
        {
            clampedY = std::min(clampedY, m_maximumY);
        }

        if (clampedX == position.x && clampedY == position.y)
        {
            return;
        }

        b2Body_SetTransform(
            bodyId,
            ToBox2DPosition(float3{ clampedX, clampedY, 0.0f }),
            b2Body_GetRotation(bodyId));
        SyncTransformFromBox2DBody();

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Screen clamp applied prevX=" << originalX
            << " prevY=" << originalY
            << " newX=" << clampedX
            << " newY=" << clampedY
            << " boundsX=[" << m_minScreenX << ',' << m_maxScreenX << ']';
        if (m_hasMaximumY)
        {
            oss << " maxY=" << m_maximumY;
        }
        oss << "\n";
        PlayerDebugTrace(oss.str());
    }
}
