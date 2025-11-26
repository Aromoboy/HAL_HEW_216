#include "Entities/Player.h"

#include "Core/Input.h"
#include "ModelCache.h"
#include "Sweets/BaseSweets.h"
#include "SweetsManager.h"
#include "SweetsPrototypeManifest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <sstream>
#include <vector>
#include <DirectXMath.h>

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

    constexpr float kDefaultPickupRadius = 2.5f;
    constexpr float kCarryVerticalOffset = 1.0f;
    constexpr float kCarryForwardOffset = 0.5f;
    constexpr float kDefaultPlayerMoveSpeed = 15.0f;

    constexpr int kDefaultMaxCarry = 3;

    std::shared_ptr<ModelDX> CreateProceduralFairyModel()
    {
        static std::weak_ptr<ModelDX> cached;
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

        auto model = std::make_shared<ModelDX>(
            std::vector<std::shared_ptr<MeshDX>>{mesh},
            std::vector<std::shared_ptr<MaterialDX>>{mat});

        cached = model;
        return model;
    }
}

namespace Entities
{
    Player::Player(const Transform& transform, std::shared_ptr<Render::Model> model, std::shared_ptr<Collision::ICollider> collider, Physics::Physical physical, Systems::PhysicsMachine* physicsMachine, Systems::ContactLiteSystem* contactSystem, SweetsSpace::SweetsManager* sweetsManager, Collision::CollisionLayer collisionLayer, bool kinematic, Core::UpdatePhase phase, int priority, std::string tag)
        : PhysicalEntity(transform, std::move(collider), collisionLayer, physical, std::move(model), physicsMachine, contactSystem, phase, priority, std::move(tag))
        , m_fairyModel(nullptr)
        , m_contactSystem(contactSystem)
        , m_sweetsManager(sweetsManager)
        , m_carrieds()
        , m_maxCarry(kDefaultMaxCarry)
        , m_pickupRadius(kDefaultPickupRadius)
        , m_carryOffset(float3{ 0, kCarryVerticalOffset, kCarryForwardOffset })
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
        if (m_isKinematic)
        {
            m_physical.weight = 0.0f;
        }

        Render::ModelCache& cache = Render::ModelCache::Instance();
        const auto& manifest = SweetsSpace::SweetsPrototypeManifest::Instance();

        const auto* d = manifest.Find(SweetsSpace::SweetsType::Fairy);
        const std::string modelPath = d ? d->modelPath : std::string(kFairyFallbackPath);

        PlayerDebugTrace(std::string(kPlayerLogPrefix)
            + "Using fairy prototype or fallback.\n");

        m_fairyModel = CreateProceduralFairyModel();
        if (!m_fairyModel)
            m_fairyModel = cache.Get(modelPath);

        if (!m_fairyModel && modelPath != kFairyFallbackPath)
            m_fairyModel = cache.Get(kFairyFallbackPath);

        if (!m_fairyModel)
            m_missingModelWarned = true;

        if (!m_model)
        {
            m_model = m_fairyModel;
        }
    }

    void Player::OnUpdate(Core::UpdateContext& ctx)
    {
        Core::Input& input = ctx.input;

        const float horizontal = input.GetAxis(Core::ActionId::MoveX);
        const float vertical = input.GetAxis(Core::ActionId::RotateYaw);

        const bool canMove = (ctx.dt > 0.0f && m_moveSpeed > 0.0f);
        if (canMove && (horizontal != 0.0f || vertical != 0.0f))
        {
            float3 translation{ 0.0f, 0.0f, 0.0f };
            translation.x = horizontal * m_moveSpeed * ctx.dt;
            translation.y = vertical * m_moveSpeed * ctx.dt;

            ApplyTranslation(translation);

            std::ostringstream oss;
            oss << kPlayerLogPrefix << "Move input hx=" << horizontal
                << " vy=" << vertical
                << " speed=" << m_moveSpeed
                << " dt=" << ctx.dt
                << " delta=(" << translation.x << ',' << translation.y << ',' << translation.z << ")\n";
            PlayerDebugTrace(oss.str());
        }

        ClampVerticalPosition();
        ClampScreenBounds();

        HandleCarryInput(ctx);
        UpdateCarriedSweets();
    }

    void Player::Draw(Render::ModelRenderer& renderer, const CameraSpace::ICamera& camera)
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

    float Player::GetMoveSpeed() const
    {
        return m_moveSpeed;
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

    float3 Player::ComputeCarryAnchor() const
    {
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

        for (auto& it : m_carrieds)
        {
            auto& sweetsTransform = it->GetTransform();
            sweetsTransform.position = ComputeCarryAnchor();
            sweetsTransform.rotation = m_transform.rotation;

            it->SyncColliderBounds();

            Physics::Physical& physical = it->GetPhysical();
            physical.velocity = { 0.0f, 0.0f, 0.0f };
            physical.force = { 0.0f, 0.0f, 0.0f };
        }

    }

    void Player::HandleCarryInput(Core::UpdateContext& ctx)
    {
        Core::Input& input = ctx.input;

        if (input.IsAction(Core::ActionId::Drop) && !m_carrieds.empty())
        {
            DropCarriedSweets();
            return;
        }

        if (input.IsAction(Core::ActionId::Confirm) && m_carrieds.size() < m_maxCarry)
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

        auto candidate = m_sweetsManager->FindNearestCarryable(m_transform.position, m_pickupRadius);
        if (!candidate)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Pickup failed: no sweets within radius.\n");
            return;
        }


        if (std::find(m_carrieds.begin(), m_carrieds.end(), candidate) != m_carrieds.end())
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "Pickup failed: sweets already carried.\n");
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

        m_carrieds.push_back(candidate);

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Pickup success tag='" << candidate->GetTag()
            << "' radius=" << m_pickupRadius << "\n";
        PlayerDebugTrace(oss.str());
    }

    void Player::DropCarriedSweets()
    {
        if (m_carrieds.empty())
        {
            return;
        };

        auto sweets = m_carrieds.back();
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

        m_carrieds.pop_back();
    }

    void Player::ClampVerticalPosition()
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

    void Player::ClampScreenBounds()
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
}
