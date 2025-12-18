#include "Entities/Player.h"

#include "AABB.h"
#include "Core/Input.h"
#include "Entities/Player.h"

#include "BoxColliderComponent.h"
#include "ModelCache.h"
#include "ModelPivot.h"
#include "Systems/PhysicsMachine.h"
#include "Sweets/BaseSweets.h"
#include "SweetsManager.h"
#include "SweetsPrototypeManifest.h"
#include "Entities/PhysicalEntity.h"


#include <algorithm>
#include <array>
#include <cmath>
#include <cassert>
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
    constexpr float kDefaultPickupHalfExtent = 0.5f;
    constexpr float kDefaultPickupMaxDistance = 0.5f;
    constexpr float kDefaultPickupDepthThreshold = std::numeric_limits<float>::infinity();

    const char* PickupOffsetModeName(Stage::PickupOffsetMode mode)
    {
        switch (mode)
        {
        case Stage::PickupOffsetMode::None: return "none";
        case Stage::PickupOffsetMode::FacingDirection: return "facing-direction";
        case Stage::PickupOffsetMode::WorldX: return "world-x";
        default: return "unknown";
        }
    }

    const char* PickupResultName(SweetsSpace::PickupResult result)
    {
        switch (result)
        {
        case SweetsSpace::PickupResult::Success: return "success";
        case SweetsSpace::PickupResult::Failure: return "failure";
        default: return "unknown";
        }
    }

    const char* PickupFailReasonName(SweetsSpace::PickupFailReason reason)
    {
        switch (reason)
        {
        case SweetsSpace::PickupFailReason::None: return "none";
        case SweetsSpace::PickupFailReason::MissingSweetsManager: return "missing-sweets-manager";
        case SweetsSpace::PickupFailReason::MissingSystems: return "missing-systems";
        case SweetsSpace::PickupFailReason::InvalidConfiguration: return "invalid-configuration";
        case SweetsSpace::PickupFailReason::NoCandidates: return "no-candidates";
        case SweetsSpace::PickupFailReason::DistanceExceeded: return "distance-exceeded";
        case SweetsSpace::PickupFailReason::DepthExceeded: return "depth-exceeded";
        case SweetsSpace::PickupFailReason::StateTransitionFailed: return "state-transition-failed";
        default: return "unknown";
        }
    }

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
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "キャッシュ済みのプロシージャルフェアリーを再利用します \n");
            return reused;
        }

        ID3D11Device* device = GetDevice();
        if (!device)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "プロシージャルフェアリーを生成できません: デバイスが存在しません \n");
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
        config.type = Physics::Box2DShapeConfig::Type::None;
        config.bodyType = isKinematic ? b2_kinematicBody : b2_dynamicBody;
        config.fixedRotation = true;
        config.allowRotation = false;
        config.angularDamping = 5.0f;
        config.density = 0.5f;
        config.friction = 0.35f;
        config.restitution = 0.05f;

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
        , m_carriedRotations()
        , m_maxCarry(kDefaultMaxCarry)
        , m_pickupHalfWidth(0.0f)
        , m_pickupHalfHeight(0.0f)
        , m_pickupOffset{}
        , m_pickupMaxDistance(kDefaultPickupMaxDistance)
        , m_pickupDepthThreshold(kDefaultPickupDepthThreshold)
        , m_pickupOriginOffset(float3{ 0.0f, 0.0f, 0.0f })
        , m_carryOffset(float3{ 0, kCarryVerticalOffset, kCarryForwardOffset })
        , m_dropUpwardBoost(4.0f)
        , m_isKinematic(kinematic)
        , m_baseHorizontalMoveSpeed(0.0f)
        , m_baseVerticalMoveSpeed(0.0f)
        , m_speedMultiplier(1.0f)
        , m_facingSign(1.0f)
        , m_hasMinimumY(false)
        , m_minimumY(0.0f)
        , m_missingBodyLogged(false)
        , m_missingModelWarned(false)
        , m_hasScreenBounds(false)
        , m_minScreenX(0.0f)
        , m_maxScreenX(0.0f)
        , m_hasMaximumY(false)
        , m_maximumY(0.0f)
        , m_lastPickupResult{}
        , m_lastCarryInputLoggedCount(m_carrieds.size())
    {
        if (model)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "指定されたプレイヤーモデルを使用します \n");
        }
        else
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "プレイヤーモデルが指定されていません フェアリープロトタイプを解決します \n");
        }

        Render::ModelCache& cache = Render::ModelCache::Instance();
        const auto& manifest = SweetsSpace::SweetsPrototypeManifest::Instance();

        const auto* descriptor = manifest.Find("Fairy");
        std::string modelPath = descriptor ? descriptor->modelPath : std::string{};
        if (!descriptor)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "フェアリーディスクリプタが見つかりません プレースホルダーにフォールバックします \n");
            modelPath = kFairyFallbackPath;
        }
        else if (modelPath.empty())
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "フェアリーディスクリプタのパスが空です プレースホルダーにフォールバックします \n");
            modelPath = kFairyFallbackPath;
        }
        else
        {
            std::ostringstream oss;
            oss << kPlayerLogPrefix << "フェアリーディスクリプタを解決しました key='" << descriptor->key
                << "' path='" << descriptor->modelPath << "'.\n";
            PlayerDebugTrace(oss.str());
        }

        m_fairyModel = CreateProceduralFairyModel();
        if (!m_fairyModel)
        {
            m_fairyModel = cache.Get(modelPath);
            if (!m_fairyModel && modelPath != kFairyFallbackPath)
            {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "フェアリーの一次モデルの読み込みに失敗しました プレースホルダーを試します \n");
                m_fairyModel = cache.Get(kFairyFallbackPath);
            }
        }

        if (!m_fairyModel)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "フォールバック後も利用可能なフェアリーモデルがありません \n");
            m_missingModelWarned = true;
        }

        if (!m_model)
        {
            m_model = m_fairyModel;
        }

        const Render::PivotInfo pivotInfo = Render::ComputePivotInfoOriginBottom(m_model, m_transform.scale);
        if (pivotInfo.offsetY != 0.0f)
        {
            SetPosition(float3{ m_transform.position.x, m_transform.position.y + pivotInfo.offsetY, m_transform.position.z });

            std::ostringstream oss;
            oss << kPlayerLogPrefix << "[ピボット] policy=" << Render::kPivotPolicyName
                << " offsetY(raw/clamped)=" << pivotInfo.rawOffsetY << '/' << pivotInfo.offsetY
                << " halfHeight=" << pivotInfo.halfHeight
                << " scaleY=" << m_transform.scale.y << '\n';
            PlayerDebugTrace(oss.str());
        }

        m_visual.position = m_transform.position;
        m_visual.rotation = m_transform.rotation;
        m_visual.scale = m_transform.scale;
        RefreshRenderTransform();

        const auto shapeConfig = BuildDefaultPlayerShapeConfig(m_transform, m_isKinematic);
        Physics::BoxColliderComponent colliderComponent;
        colliderComponent.colliderId = descriptor ? descriptor->key : std::string{ "Fairy" };

        SetBoxColliderComponent(std::move(colliderComponent));
        SetBox2DShapeConfig(shapeConfig);
    }

    void Player::OnUpdate(Core::UpdateContext& ctx)
    {
        SyncVisualTransformFromPhysics();

        Core::Input& input = ctx.input;

        const bool hasBody = HasBox2DBody();
        if (!hasBody && !m_missingBodyLogged)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "[Fatal] Missing Box2D body; falling back to transform-based update for pickups to continue.\n");
            m_missingBodyLogged = true;
        }

        const float horizontal = input.GetAxis(Core::ActionId::MoveX);
        const float vertical = input.GetAxis(Core::ActionId::RotateYaw);

        if (horizontal > 0.0f)
        {
            m_facingSign = 1.0f;
        }
        else if (horizontal < 0.0f)
        {
            m_facingSign = -1.0f;
        }

        const float effectiveHorizontalSpeed = m_baseHorizontalMoveSpeed * m_speedMultiplier;
        const float effectiveVerticalSpeed = m_baseVerticalMoveSpeed * m_speedMultiplier;

        if (ctx.dt > 0.0f && (effectiveHorizontalSpeed > 0.0f || effectiveVerticalSpeed > 0.0f))
        {
            if (horizontal != 0.0f || vertical != 0.0f)
            {
                if (hasBody)
                {
                    const float3 desired{
                        horizontal * effectiveHorizontalSpeed,
                        vertical * effectiveVerticalSpeed,
                        0.0f
                    };

                    SetLinearVelocity(desired);
                }
                else
                {
                    const float3 delta{
                        horizontal * effectiveHorizontalSpeed * ctx.dt,
                        vertical * effectiveVerticalSpeed * ctx.dt,
                        0.0f
                    };
                    ApplyTranslation(delta);
                }
            }
            else if (hasBody)
            {
                SetLinearVelocity(float3{ 0,0,0 });
            }
        }

        ClampVerticalPosition();
        ClampScreenBounds();
        SyncVisualTransformFromPhysics();
        HandleCarryInput(ctx);
        UpdateCarriedSweets();
    }

    void Player::Draw(Render::ModelRenderer& renderer, const ::CameraSpace::ICamera& camera)
    {
        if (!m_model)
        {
            if (!m_missingModelWarned)
            {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "描画をスキップしました: プレイヤーモデルがありません \n");
                m_missingModelWarned = true;
            }
            return;
        }

        renderer.Draw(*m_model, m_renderTransform, &camera);
    }

    Transform Player::GetRenderTransform() const
    {
        return m_renderTransform;
    }

    const SweetsSpace::PickupQueryResult& Player::GetLastPickupResult() const noexcept
    {
        return m_lastPickupResult;
    }

    void Player::SyncVisualTransformFromPhysics()
    {
        if (HasBox2DBody())
        {
            SyncTransformFromBox2DBody();
        }

        m_visual.position = m_transform.position;
        m_visual.rotation = m_transform.rotation;
        m_visual.scale = m_transform.scale;
        RefreshRenderTransform();
    }

    void Player::RefreshRenderTransform()
    {
        // 描画時に再計算せず、最新のビジュアル変換と整合した描画登録を維持します 
        m_renderTransform = Transform{ m_visual.position, m_visual.rotation, m_visual.scale };
    }

    void Player::SetSweetsManager(SweetsSpace::SweetsManager* manager)
    {
        if (m_sweetsManager == manager)
        {
            return;
        }

        if (!manager && !m_carrieds.empty())
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "保持中に SweetsManager が解放されました 強制的にリリースします \n");
            m_carrieds.clear();
            m_carriedRotations.clear();
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
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "保持中のスイーツの参照をクリアします \n");
        }
        m_carrieds.clear();
        m_carriedRotations.clear();
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
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "非有限の移動速度指定を無視しました \n");
            return;
        }

        const float sanitizedHorizontal = std::max(0.0f, horizontal);
        const float sanitizedVertical = std::max(0.0f, vertical);

        if (sanitizedHorizontal != horizontal || sanitizedVertical != vertical)
        {
            std::ostringstream oss;
            oss << kPlayerLogPrefix << "移動速度を (" << horizontal << ',' << vertical
                << ") から (" << sanitizedHorizontal << ',' << sanitizedVertical << ") に正規化しました（負値は不可） \n";
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
        oss << kPlayerLogPrefix << "基準移動速度を (" << m_baseHorizontalMoveSpeed
            << ", " << m_baseVerticalMoveSpeed << ") units/sec に設定しました \n";
        PlayerDebugTrace(oss.str());
    }

    void Player::SetSpeedMultiplier(float multiplier)
    {
        if (!std::isfinite(multiplier))
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "非有限の速度倍率指定を無視しました \n");
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
        oss << kPlayerLogPrefix << "速度倍率を " << m_speedMultiplier << "x に設定しました \n";
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
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "非有限の最小 Y 指定を無視しました \n");
            return;
        }

        if (m_hasMinimumY && std::fabs(m_minimumY - minY) < std::numeric_limits<float>::epsilon())
        {
            return;
        }

        m_hasMinimumY = true;
        m_minimumY = minY;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "最小 Y を " << m_minimumY << " 単位に設定しました \n";
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
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "非有限のスクリーン範囲を無視しました \n");
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
        oss << kPlayerLogPrefix << "スクリーン範囲を minX=" << m_minScreenX
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
        SetPickupArea(width, height, m_pickupOffset);
    }

    void Player::SetPickupArea(float width, float height, const Stage::PickupOffset& offset)
    {
        if (!std::isfinite(width) || !std::isfinite(height)
            || !std::isfinite(offset.localOffset.x)
            || !std::isfinite(offset.localOffset.y)
            || !std::isfinite(offset.localOffset.z))
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "非有限のピックアップボックス指定を無視しました \n");
            return;
        }

        Stage::PickupOffset sanitizedOffset = offset;
        switch (sanitizedOffset.mode)
        {
        case Stage::PickupOffsetMode::None:
        case Stage::PickupOffsetMode::FacingDirection:
        case Stage::PickupOffsetMode::WorldX:
            break;
        default:
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "未知のピックアップオフセットモードを WorldX にフォールバックします \n");
            sanitizedOffset.mode = Stage::PickupOffsetMode::WorldX;
            break;
        }

        const float sanitizedWidth = std::max(0.0f, width);
        const float sanitizedHeight = std::max(0.0f, height);
        const float halfWidth = sanitizedWidth * 0.5f;
        const float halfHeight = sanitizedHeight * 0.5f;

        const bool unchanged = std::fabs(m_pickupHalfWidth - halfWidth) < std::numeric_limits<float>::epsilon()
            && std::fabs(m_pickupHalfHeight - halfHeight) < std::numeric_limits<float>::epsilon()
            && m_pickupOffset.mode == sanitizedOffset.mode
            && std::fabs(m_pickupOffset.localOffset.x - sanitizedOffset.localOffset.x) < std::numeric_limits<float>::epsilon()
            && std::fabs(m_pickupOffset.localOffset.y - sanitizedOffset.localOffset.y) < std::numeric_limits<float>::epsilon()
            && std::fabs(m_pickupOffset.localOffset.z - sanitizedOffset.localOffset.z) < std::numeric_limits<float>::epsilon();
        if (unchanged)
        {
            return;
        }

        m_pickupHalfWidth = halfWidth;
        m_pickupHalfHeight = halfHeight;
        m_pickupOffset = sanitizedOffset;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "ピックアップボックスを width=" << sanitizedWidth
            << " height=" << sanitizedHeight << " (half=" << m_pickupHalfWidth
            << ',' << m_pickupHalfHeight << ") offsetMode=" << PickupOffsetModeName(m_pickupOffset.mode)
            << " localOffset=(" << m_pickupOffset.localOffset.x << ',' << m_pickupOffset.localOffset.y
            << ',' << m_pickupOffset.localOffset.z << ") に設定しました \n";
        PlayerDebugTrace(oss.str());
    }

    void Player::SetPickupMaxDistance(float maxDistance)
    {
        if (!std::isfinite(maxDistance) || maxDistance <= 0.0f)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "無効なピックアップ距離を無視しました \n");
            return;
        }

        if (std::fabs(m_pickupMaxDistance - maxDistance) < std::numeric_limits<float>::epsilon())
        {
            return;
        }

        m_pickupMaxDistance = maxDistance;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "ピックアップ最大距離を " << m_pickupMaxDistance << " に設定しました \n";
        PlayerDebugTrace(oss.str());
    }

    void Player::SetPickupDepthThreshold(float depthThreshold)
    {
        if (!std::isfinite(depthThreshold) || depthThreshold < 0.0f)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "無効なピックアップ深度しきい値を無視しました \n");
            return;
        }

        if (std::fabs(m_pickupDepthThreshold - depthThreshold) < std::numeric_limits<float>::epsilon())
        {
            return;
        }

        m_pickupDepthThreshold = depthThreshold;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "ピックアップ深度しきい値を " << m_pickupDepthThreshold << " に設定しました \n";
        PlayerDebugTrace(oss.str());
    }

    void Player::SetPickupOriginOffset(const float3& offset)
    {
        if (!std::isfinite(offset.x) || !std::isfinite(offset.y) || !std::isfinite(offset.z))
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "非有限のピックアップ基準オフセットを無視しました \n");
            return;
        }

        const bool unchanged = std::fabs(m_pickupOriginOffset.x - offset.x) < std::numeric_limits<float>::epsilon()
            && std::fabs(m_pickupOriginOffset.y - offset.y) < std::numeric_limits<float>::epsilon()
            && std::fabs(m_pickupOriginOffset.z - offset.z) < std::numeric_limits<float>::epsilon();
        if (unchanged)
        {
            return;
        }

        m_pickupOriginOffset = offset;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "ピックアップ基準を transform オフセット(" << m_pickupOriginOffset.x
            << ',' << m_pickupOriginOffset.y << ',' << m_pickupOriginOffset.z << ") で更新しました \n";
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

    Player::PickupOrigin Player::ComputePickupOrigin() const
    {
        PickupOrigin origin{};

        const float3 basePosition = HasBox2DBody()
            ? ToWorldPosition(b2Body_GetPosition(GetBox2DBodyId()))
            : m_transform.position;

        origin.center = float3{ basePosition.x + m_pickupOriginOffset.x, basePosition.y + m_pickupOriginOffset.y, basePosition.z + m_pickupOriginOffset.z };
        origin.halfDepth = 0.05f;
        origin.source = "TransformOffset";

        if (m_colliderFootprint)
        {
            origin.center = float3
            {
                basePosition.x + m_colliderFootprint->centerOffset.x,
                basePosition.y + m_colliderFootprint->centerOffset.y,
                basePosition.z + m_colliderFootprint->centerOffset.z,
            };
            origin.halfDepth = std::max(0.05f, m_colliderFootprint->halfExtents.z + 0.05f);
            origin.source = "ColliderFootprint";
        }

        return origin;
    }

    void Player::UpdateCarriedSweets()
    {
        if (m_carrieds.empty())
        {
            return;
        }

        for (auto& it : m_carrieds)
        {
            const Transform current = it ? it->GetTransform() : Transform{};
            Transform updated = current;
            updated.position = ComputeCarryAnchor();

            const auto rotationIt = m_carriedRotations.find(it.get());
            if (rotationIt != m_carriedRotations.end())
            {
                updated.rotation = rotationIt->second;
            }

            it->SetTransform(updated);
            it->SetLinearVelocity({ 0.0f, 0.0f, 0.0f });
        }

    }

    void Player::HandleCarryInput(Core::UpdateContext& ctx)
    {
        Core::Input& input = ctx.input;

        const bool dropTriggered = input.IsTrigger(Core::ActionId::Drop);
        const bool confirmTriggered = input.IsTrigger(Core::ActionId::Confirm);

        const bool carriedChanged = m_carrieds.size() != m_lastCarryInputLoggedCount;
        const bool shouldLogEntry = carriedChanged || dropTriggered || confirmTriggered;

        if (shouldLogEntry)
        {
            std::ostringstream oss;
            oss << kPlayerLogPrefix << "HandleCarryInput enter carried=" << m_carrieds.size()
                << "/" << m_maxCarry
                << " dropTrigger=" << dropTriggered
                << " confirmTrigger=" << confirmTriggered << "\n";
            PlayerDebugTrace(oss.str());

            m_lastCarryInputLoggedCount = m_carrieds.size();
        }

        if (dropTriggered)
        {
            std::ostringstream oss;
            oss << kPlayerLogPrefix << "Drop evaluated carried=" << m_carrieds.size()
                << " maxCarry=" << m_maxCarry << "\n";
            PlayerDebugTrace(oss.str());
        }

        if (dropTriggered && !m_carrieds.empty())
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "DropCarriedSweets invoked\n");
            DropCarriedSweets();
            return;
        }

        if (confirmTriggered)
        {
            std::ostringstream oss;
            oss << kPlayerLogPrefix << "Confirm evaluated carried=" << m_carrieds.size()
                << " maxCarry=" << m_maxCarry << "\n";
            PlayerDebugTrace(oss.str());
        }

        if (confirmTriggered && m_carrieds.size() < m_maxCarry)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "TryPickupNearest invoked\n");
            TryPickupNearest();
        }
    }

    void Player::TryPickupNearest()
    {
        SweetsSpace::PickupQueryResult pickupResult{};
        std::shared_ptr<SweetsSpace::ActiveSweets> candidate;
        Transform candidateTransform{};
        AABB sweetsBounds{};
        float sweetsZ = std::numeric_limits<float>::quiet_NaN();

        PickupOrigin pickupOrigin{};
        float3 pickupCenter = pickupOrigin.center;
        float halfDepth = pickupOrigin.halfDepth;
        float depthThreshold = m_pickupDepthThreshold;
        bool pickupVolumeValid = false;

        const auto clampPickupExtent = [](float value)
        {
            const float sanitized = std::isfinite(value) ? value : kDefaultPickupHalfExtent;
            return std::max(sanitized, kDefaultPickupHalfExtent);
        };

        const auto clampPickupDistance = [](float value)
        {
            const float sanitized = std::isfinite(value) ? value : kDefaultPickupMaxDistance;
            return std::max(sanitized, kDefaultPickupMaxDistance);
        };

        const float pickupHalfWidth = clampPickupExtent(m_pickupHalfWidth);
        const float pickupHalfHeight = clampPickupExtent(m_pickupHalfHeight);
        const float pickupMaxDistance = clampPickupDistance(m_pickupMaxDistance);

        if (!m_sweetsManager)
        {
            pickupResult.failReason = SweetsSpace::PickupFailReason::MissingSweetsManager;
        }
        else if (!(pickupHalfWidth > 0.0f) || !(pickupHalfHeight > 0.0f) || !(pickupMaxDistance > 0.0f))
        {
            pickupResult.failReason = SweetsSpace::PickupFailReason::InvalidConfiguration;
        }
        else
        {
            pickupOrigin = ComputePickupOrigin();
            halfDepth = pickupOrigin.halfDepth;
            depthThreshold = m_pickupDepthThreshold;

            float3 pickupOffset{ 0.0f, 0.0f, 0.0f };
            switch (m_pickupOffset.mode)
            {
            case Stage::PickupOffsetMode::FacingDirection:
                pickupOffset.x = m_pickupOffset.localOffset.x * m_facingSign;
                pickupOffset.y = m_pickupOffset.localOffset.y;
                pickupOffset.z = m_pickupOffset.localOffset.z;
                break;

            case Stage::PickupOffsetMode::WorldX:
                pickupOffset = m_pickupOffset.localOffset;
                break;

            case Stage::PickupOffsetMode::None:
            default:
                break;
            }

            pickupCenter = float3
            {
                pickupOrigin.center.x + pickupOffset.x,
                pickupOrigin.center.y + pickupOffset.y,
                pickupOrigin.center.z + pickupOffset.z
            };

            const bool depthThresholdValid = std::isfinite(depthThreshold)
                ? depthThreshold >= 0.0f
                : std::isinf(depthThreshold);

            pickupVolumeValid = std::isfinite(halfDepth) && halfDepth > 0.0f
                && depthThresholdValid;

            if (!pickupVolumeValid)
            {
                pickupResult.failReason = SweetsSpace::PickupFailReason::InvalidConfiguration;

                std::ostringstream oss;
                oss << kPlayerLogPrefix << "ピックアップボリュームが無効です halfDepth=" << halfDepth
                    << " depthThreshold=" << depthThreshold << "\n";
                PlayerDebugTrace(oss.str());
            }
            else
            {
                pickupResult = m_sweetsManager->FindNearestCarryable(
                    pickupCenter,
                    pickupHalfWidth,
                    pickupHalfHeight,
                    halfDepth,
                    depthThreshold,
                    m_carrieds,
                    pickupMaxDistance);

                candidate = pickupResult.sweets;
                if (pickupResult.Succeeded())
                {
                    candidateTransform = candidate->GetTransform();
                    sweetsBounds = candidate->GetAABB();
                    sweetsZ = candidateTransform.position.z;

                    if (!m_sweetsManager->TryMarkAsCarried(candidate))
                    {
                        pickupResult.result = SweetsSpace::PickupResult::Failure;
                        pickupResult.failReason = SweetsSpace::PickupFailReason::StateTransitionFailed;
                        candidate.reset();
                    }
                    else
                    {
                        const float3 originalRotation = candidateTransform.rotation;
                        m_carriedRotations[candidate.get()] = originalRotation;

                        Transform updated = candidateTransform;
                        updated.position = ComputeCarryAnchor();
                        updated.rotation = originalRotation;
                        candidate->SetTransformViaPhysics(updated.position, updated.rotation);
                        candidate->SetLinearVelocity({ 0.0f, 0.0f, 0.0f });

                        m_carrieds.push_back(candidate);
                    }
                }
                else
                {
                    std::shared_ptr<SweetsSpace::ActiveSweets> nearest;
                    float nearestDistanceSq = std::numeric_limits<float>::max();

                    const auto evaluateList = [&](const SweetsSpace::SweetsManager::SweetsList& list)
                    {
                        for (const auto& sweets : list)
                        {
                            if (!sweets || sweets->IsPendingDestroy())
                            {
                                continue;
                            }

                            if (std::find(m_carrieds.begin(), m_carrieds.end(), sweets) != m_carrieds.end())
                            {
                                continue;
                            }

                            const Transform sweetsTransform = sweets->GetTransform();
                            const float dx = sweetsTransform.position.x - pickupCenter.x;
                            const float dy = sweetsTransform.position.y - pickupCenter.y;
                            const float distanceSq = (dx * dx) + (dy * dy);
                            if (distanceSq < nearestDistanceSq)
                            {
                                nearestDistanceSq = distanceSq;
                                nearest = sweets;
                            }
                        }
                    };

                    evaluateList(m_sweetsManager->GetByState(SweetsSpace::SweetsState::Ground));
                    evaluateList(m_sweetsManager->GetByState(SweetsSpace::SweetsState::Active));
                    evaluateList(m_sweetsManager->GetByState(SweetsSpace::SweetsState::Stacked));

                    if (nearest)
                    {
                        const float nearestDistance = std::sqrt(nearestDistanceSq);
                        pickupResult.distance = nearestDistance;

                        pickupResult.result = SweetsSpace::PickupResult::Success;
                        pickupResult.failReason = SweetsSpace::PickupFailReason::None;
                        candidate = nearest;
                        candidateTransform = candidate->GetTransform();
                        sweetsBounds = candidate->GetAABB();
                        sweetsZ = candidateTransform.position.z;

                        if (!m_sweetsManager->TryMarkAsCarried(candidate))
                        {
                            pickupResult.result = SweetsSpace::PickupResult::Failure;
                            pickupResult.failReason = SweetsSpace::PickupFailReason::StateTransitionFailed;
                            candidate.reset();
                        }
                        else
                        {
                            const float3 originalRotation = candidateTransform.rotation;
                            m_carriedRotations[candidate.get()] = originalRotation;

                            Transform updated = candidateTransform;
                            updated.position = ComputeCarryAnchor();
                            updated.rotation = originalRotation;
                            candidate->SetTransformViaPhysics(updated.position, updated.rotation);
                            candidate->SetLinearVelocity({ 0.0f, 0.0f, 0.0f });

                            m_carrieds.push_back(candidate);
                        }
                    }
                }
            }
        }

        m_lastPickupResult = pickupResult;

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "ピックアップ結果 result=" << PickupResultName(pickupResult.result)
            << " reason=" << PickupFailReasonName(pickupResult.failReason)
            << " boxHalf=(" << pickupHalfWidth << ',' << pickupHalfHeight << ")"
            << " offsetMode=" << PickupOffsetModeName(m_pickupOffset.mode)
            << " offsetLocal=(" << m_pickupOffset.localOffset.x << ',' << m_pickupOffset.localOffset.y
            << ',' << m_pickupOffset.localOffset.z << ")"
            << " maxDistance=" << pickupMaxDistance;

        if (pickupVolumeValid)
        {
            oss << " origin=" << pickupOrigin.source
                << " originCenter=(" << pickupOrigin.center.x << ',' << pickupOrigin.center.y << ',' << pickupOrigin.center.z << ')'
                << " originHalfDepth=" << halfDepth
                << " depthThreshold=" << depthThreshold
                << " pickupCenterZ=" << pickupCenter.z;
        }

        if (candidate)
        {
            oss << " tag='" << candidate->GetTag() << "'"
                << " sweetsZ=" << sweetsZ
                << " sweetsAABBZ=[" << sweetsBounds.min.z << ',' << sweetsBounds.max.z << ']';
        }
        if (std::isfinite(pickupResult.distance))
        {
            oss << " distance=" << pickupResult.distance;
        }
        if (std::isfinite(pickupResult.depthDelta))
        {
            oss << " depthDelta=" << pickupResult.depthDelta;
        }
        oss << "\n";
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

        m_carriedRotations.erase(sweets.get());

        const float3 bodyVelocity = GetLinearVelocity();
        float3 releaseVelocity = bodyVelocity;
        releaseVelocity.y = std::max(releaseVelocity.y, m_dropUpwardBoost);
        sweets->SetLinearVelocity(releaseVelocity);

        if (m_sweetsManager)
        {
            if (!m_sweetsManager->ReleaseCarriedSweets(sweets))
            {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "ドロップ警告: マネージャーが解放を拒否しました \n");
            }
        }
        else
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "ドロップ警告: SweetsManager がありません \n");
        }

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "ドロップ成功 tag='" << sweets->GetTag()
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

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "Y を " << previousY << " から最小 "
            << m_minimumY << " に制限しました（プラットフォーム境界） \n";
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

        std::ostringstream oss;
        oss << kPlayerLogPrefix << "スクリーンクランプを適用しました prevX=" << originalX
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
