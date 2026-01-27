#include "Entities/PlayerModel.h"

#include "AABB.h"
#include "AnimationCache.h"
#include "Entities/Player.h"
#include "ModelCache.h"
#include "ModelPivot.h"
#include "DirectX.h"
#include "ModelDX.h"
#include "MeshDX.h"
#include "MaterialDX.h"
#include "SweetsPrototypeManifest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
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
    constexpr const char* kPlayerMaterialKey = "player";
    constexpr const char* kFairyMaterialKey = "fairy";

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
        mat->SetMaterialKey(kFairyMaterialKey);
        ApplyMaterialTuning(
            *mat,
            kFairyMaterialKey,
            Render::ModelCache::Instance().GetMaterialTuningSettings());

        const std::optional<AABB> bounds = ComputeBounds(vertices);

        auto model = std::make_shared<ModelDX>(
            std::vector<std::shared_ptr<MeshDX>>{mesh},
            std::vector<std::shared_ptr<MaterialDX>>{mat}
        );

        if (bounds)
        {
            model->SetLocalBounds(*bounds);
        }

        cached = model;
        return model;
    }
}

namespace Entities
{
    void PlayerModel::Initialize(Player& player, std::shared_ptr<Render::Model> model)
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
        const std::string modelPath = descriptor ? descriptor->modelPath : std::string{};
        if (!descriptor)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "フェアリーディスクリプタが見つかりません プロシージャルモデルにフォールバックします \n");
        }
        else if (modelPath.empty())
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "フェアリーディスクリプタのパスが空です プロシージャルモデルにフォールバックします \n");
        }
        else
        {
            std::ostringstream oss;
            oss << kPlayerLogPrefix << "フェアリーディスクリプタを解決しました key='" << descriptor->key
                << "' path='" << descriptor->modelPath << "'.\n";
            PlayerDebugTrace(oss.str());
        }

        Render::AnimationCache& animationCache = Render::AnimationCache::Instance();

        const std::string playerIdleAnimationPath = "Assets/Animation/yousei_IdleAnimations.fbx";
        const std::string playerStabAnimationPath = "Assets/Animation/yousei_StabAnimations.fbx";

        m_idleAnimation = animationCache.Get(playerIdleAnimationPath);
        m_stabAnimation = animationCache.Get(playerStabAnimationPath);

        if (m_idleAnimation)
        {
            m_idleAnimation->SetLoop(true);
        }

        if (m_stabAnimation)
        {
            m_stabAnimation->SetLoop(false);
        }

        m_activeAnimation = m_idleAnimation;
        player.m_animation = m_activeAnimation;

        if (!modelPath.empty())
        {
            m_fairyModel = cache.Get(modelPath);
        }

        if (!m_fairyModel)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "フェアリーモデルの読み込みに失敗しました プロシージャルモデルを生成します \n");
            m_fairyModel = CreateProceduralFairyModel();
        }

        if (!m_fairyModel)
        {
            PlayerDebugTrace(std::string(kPlayerLogPrefix) + "フォールバック後も利用可能なフェアリーモデルがありません \n");
            m_missingModelWarned = true;
        }

        std::shared_ptr<Render::Model> selectedModel = model ? std::move(model) : player.m_model;
        if (!selectedModel)
        {
            selectedModel = m_fairyModel;
        }
        const bool usesFairyModel = (selectedModel == m_fairyModel);
        player.m_model = std::move(selectedModel);
        if (player.m_model)
        {
            player.m_model->SetMaterialKey(usesFairyModel ? kFairyMaterialKey : kPlayerMaterialKey);
        }

        const Render::PivotInfo pivotInfo = Render::ComputePivotInfoOriginBottom(player.m_model, player.m_transform.scale);
        if (pivotInfo.offsetY != 0.0f)
        {
            player.SetPosition(float3{ player.m_transform.position.x, player.m_transform.position.y + pivotInfo.offsetY, player.m_transform.position.z });

            std::ostringstream oss;
            oss << kPlayerLogPrefix << "[ピボット] policy=" << Render::kPivotPolicyName
                << " offsetY(raw/clamped)=" << pivotInfo.rawOffsetY << '/' << pivotInfo.offsetY
                << " halfHeight=" << pivotInfo.halfHeight
                << " scaleY=" << player.m_transform.scale.y << '\n';
            PlayerDebugTrace(oss.str());
        }

        m_visual.position = player.m_transform.position;
        m_visual.rotation = player.m_transform.rotation;
        m_visual.scale = player.m_transform.scale;
        m_visual.scale.x = std::abs(m_visual.scale.x) * player.GetFacingSign();
        RefreshRenderTransform();
    }

    void PlayerModel::SyncVisualTransformFromPhysics(Player& player)
    {
        if (player.HasBox2DBody())
        {
            player.SyncTransformFromBox2DBody();
        }

        m_visual.position = player.m_transform.position;
        m_visual.rotation = player.m_transform.rotation;
        m_visual.scale = player.m_transform.scale;
        m_visual.scale.x = std::abs(m_visual.scale.x) * player.GetFacingSign();
        RefreshRenderTransform();
    }

    void PlayerModel::UpdateAnimation(Player& player)
    {
        if (m_activeAnimation)
        {
            m_activeAnimation->Step(1.0f / 60.0f);

            if (m_activeAnimation == m_stabAnimation && m_activeAnimation->IsPlayEnd())
            {
                m_activeAnimation = m_idleAnimation;
                player.m_animation = m_activeAnimation;
                if (m_idleAnimation)
                {
                    m_idleAnimation->SetLoop(true);
                }
            }
        }
    }

    void PlayerModel::Draw(const Player& player, Render::ModelRenderer& renderer, const CameraSpace::ICamera& camera)
    {
        if (!player.m_model)
        {
            if (!m_missingModelWarned)
            {
                PlayerDebugTrace(std::string(kPlayerLogPrefix) + "描画をスキップしました: プレイヤーモデルがありません \n");
                m_missingModelWarned = true;
            }
            return;
        }

        renderer.Draw(*player.m_model, m_renderTransform, &camera, m_activeAnimation.get());
    }

    void PlayerModel::PlayStabAnimation(Player& player)
    {
        m_activeAnimation = m_stabAnimation;
        player.m_animation = m_activeAnimation;
        if (m_activeAnimation)
        {
            m_activeAnimation->SetLoop(false);
            m_activeAnimation->Play();
        }
    }

    Transform PlayerModel::GetRenderTransform() const
    {
        return m_renderTransform;
    }

    std::shared_ptr<Animation> PlayerModel::GetActiveAnimation() const
    {
        return m_activeAnimation;
    }

    void PlayerModel::RefreshRenderTransform()
    {
        m_renderTransform = Transform{ m_visual.position, m_visual.rotation, m_visual.scale };
    }
}
