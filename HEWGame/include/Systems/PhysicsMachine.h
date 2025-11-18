// ============================================================================
// 概要: PhysicsMachine に関するインターフェースやデータ構造を宣言し、include／Systems から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include／Systems 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#pragma once

#include "Core/UpdateNode.h"

#include <memory>
#include <vector>

#include "PhysicalEntity.h"

namespace Systems

{
    class IPhysicsSystem



    {
    public:
        virtual ~IPhysicsSystem() = default;
        virtual void Apply(SceneSpace::PhysicalEntity& entity, float dt) = 0;
    };

    class IntegrateSystem;

    class PhysicsMachine : public Core::UpdateNode
    {
    public:
        using SystemPtr = std::shared_ptr<IPhysicsSystem>;
        using IntegratorPtr = std::shared_ptr<IntegrateSystem>;

        PhysicsMachine();
        ~PhysicsMachine() override = default;

        void SetSystem(SystemPtr system);
        void SetIntegrator(IntegratorPtr integrator);
        SystemPtr GetSystem() const;
        IntegratorPtr GetIntegrator() const;
        const std::vector<SceneSpace::PhysicalEntity*>& GetBodies() const;

        void RegisterBody(SceneSpace::PhysicalEntity& body);
        void UnregisterBody(SceneSpace::PhysicalEntity& body);
        void ClearBodies();

        void OnUpdate(Core::UpdateContext& ctx) override;

    private:
        SystemPtr m_system;
        IntegratorPtr m_integrator;
        std::vector<SceneSpace::PhysicalEntity*> m_bodies;
        bool m_loggedMissingSystem;
        bool m_loggedMissingIntegrator;
    };
}
