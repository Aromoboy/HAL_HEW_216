// ガイドラインの表示用オブジェクトを扱う
// ガイドライン更新と描画から参照される

#pragma once

#include "Entities/Entity.h"

class GuidelineActor : public Entities::Entity
{
public:
    GuidelineActor();

    void UpdateGuideline(float centerX, float height, float planeZ, float width, float thickness);
};
