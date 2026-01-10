#pragma once

#include <memory>

#include "Entities/Entity.h"
#include "Model.h"

class GuidelineActor : public Entities::Entity
{
public:
    GuidelineActor();
    explicit GuidelineActor(std::shared_ptr<Render::Model> model);

    void UpdateGuideline(float centerX, float height, float planeZ, float width, float thickness);
};
