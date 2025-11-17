#pragma once

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>

#include "Core/UpdateNode.h"
#include "ICamera.h"
#include "ModelRender.h"
#include "Transform.h"

namespace SweetsSpace
{
    class BaseSweets;

    enum class SweetsState : std::uint8_t
    {
        Active,  // 現在制御中または落下中の生成されたお菓子
        Stacked, // タワーの一部であるお菓子を積み重ねることに成功した
        Ground   // 地面に置かれたお菓子が、拾われるのを待っている
    };

    class SweetsManager : public Core::UpdateNode
    {
    public:
        using SweetsList = std::list<std::shared_ptr<BaseSweets>>;

        SweetsManager();
        ~SweetsManager();

        void OnUpdate(Core::UpdateContext& ctx) override;

        void DrawAll(Render::ModelRenderer& renderer, const std::shared_ptr<CameraSpace::ICamera>& camera);

        SweetsList& GetByState(SweetsState state);
        const SweetsList& GetByState(SweetsState state) const;

        void SetState(const std::shared_ptr<BaseSweets>& sweets, SweetsState state);
        void Erase(const std::shared_ptr<BaseSweets>& sweets);

        std::shared_ptr<BaseSweets> Spawn(const std::string& kind, Transform transform);

    private:
        struct SweetsStateHash
        {
            std::size_t operator()(SweetsState state) const noexcept
            {
                return static_cast<std::size_t>(state);
            }
        };

        std::unordered_map<SweetsState, SweetsList, SweetsStateHash> m_byState;
        std::unordered_map<std::shared_ptr<BaseSweets>, SweetsState> m_lookup;

        std::unordered_map<std::string, std::function<std::shared_ptr<BaseSweets>(Transform)>> m_spawner;

        template<typename T>
        std::shared_ptr<BaseSweets> MakeSweets(Transform transform)
        {
            return std::make_shared<T>(transform.position, transform.rotation, transform.scale /*etx...*/);
        }
    };
}
