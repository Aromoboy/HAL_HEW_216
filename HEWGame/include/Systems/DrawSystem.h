// ============================================================================
// 概要: DrawSystem に関するインターフェースやデータ構造を宣言し、include／Systems から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include／Systems 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#pragma once

#include "Core/UpdateNode.h"

#include <memory>
#include <unordered_set>
#include <vector>

#include "Transform.h"

namespace CameraSpace
{
        class ICamera;
}

class ModelDX;
class ModelRender;

namespace SceneSpace
{
        class Entity;
}

namespace Systems
{
        class DrawSystem : public Core::UpdateNode
        {
        public:
                DrawSystem();
                ~DrawSystem() override = default;

                void SetRenderer(ModelRender* renderer);
                void RegisterRenderable(SceneSpace::Entity& entity);
                void UnregisterRenderable(SceneSpace::Entity& entity);
                void ClearRenderables();

                void SetDebugLoggingEnabled(bool enabled);

                void OnUpdate(Core::UpdateContext& ctx) override;
                void RenderQueued(const std::shared_ptr<CameraSpace::ICamera>& camera);

                size_t GetRenderCommandCount() const;

        private:
                struct DrawCommand
                {
                        SceneSpace::Entity* source;
                        ModelDX* model;
                        Transform transform;
                };

                bool ValidateTransform(const Transform& transform, const SceneSpace::Entity* source) const;
                void LogMissingRendererOnce();
                void LogMissingCameraOnce();
                void LogMissingModel(SceneSpace::Entity* entity);

                ModelRender* m_renderer;
                std::vector<SceneSpace::Entity*> m_renderables;
                std::vector<DrawCommand> m_commands;
                std::unordered_set<const SceneSpace::Entity*> m_missingModelEntities;
                bool m_debugLoggingEnabled;
                bool m_warnedMissingRenderer;
                bool m_warnedMissingCamera;
        };
}
