#pragma once

#include "Transform.h"

#include <string>

class ModelRender;

namespace SceneSpace
{
        class Entity
        {
        public:
                explicit Entity(std::string tag = {},
                        const float3& position = { 0.0f, 0.0f, 0.0f },
                        const float3& rotation = { 0.0f, 0.0f, 0.0f },
                        const float3& scale = { 1.0f, 1.0f, 1.0f });
                virtual ~Entity();

                virtual void OnUpdate();
                virtual void Draw(ModelRender& renderer);

                Transform& GetTransform();
                const Transform& GetTransform() const;

        private:
                Transform m_transform;
                std::string m_tag;
        };
}
