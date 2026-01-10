#pragma once

#include <memory>

#include "Entities/Entity.h"
#include "Model.h"

class GuidelineActor : public Entities::Entity
{
public:
    GuidelineActor();
    explicit GuidelineActor(std::shared_ptr<Render::Model> model);

    void OnUpdate(const Core::UpdateContext& ctx) override;
    void UpdateGuideline(float centerX, float height, float planeZ, float width, float thickness);
    void AdvanceSpin(float deltaSeconds);
    void SetSpin(const float3& axis, float speedDegPerSec);

private:
    void ApplyRotation();

    float3 m_baseRotation{ 30.0f, 0.0f, 0.0f };
    float3 m_spinAxis{ 0.0f, 1.0f, 0.0f };
    float m_spinSpeedDegPerSec = 0.0f;
    float m_spinAngleRadians = 0.0f;
};
