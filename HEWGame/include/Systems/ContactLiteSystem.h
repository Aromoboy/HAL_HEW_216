// ============================================================================
// 概要: ContactLiteSystem に関するインターフェースやデータ構造を宣言し、include／Systems から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include／Systems 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#pragma once

#include "Core/UpdateNode.h"

#include <vector>

#include "PhysicalEntity.h"

namespace Systems
{
    class ContactLiteSystem : public Core::UpdateNode
    {
    public:
        ContactLiteSystem();
        ~ContactLiteSystem() override = default;

        void SetBodySource(const std::vector<SceneSpace::PhysicalEntity*>* bodies);
        void SetPenetrationTolerance(float tolerance);
        void SetRestitution(float restitution);

        float GetPenetrationTolerance() const { return m_penetrationTolerance; }
        float GetRestitution() const { return m_restitution; }

        void OnUpdate(Core::UpdateContext& ctx) override;

    private:
        const std::vector<SceneSpace::PhysicalEntity*>* m_bodies;
        float m_penetrationTolerance;
        float m_restitution;
        bool m_loggedMissingBodyList;
        bool m_loggedPlayerPresence;
        bool m_loggedPlayerAbsence;
        bool m_loggedPlayerPlatformContact;
        bool m_loggedPlayerSweetsContact;
    };
}
