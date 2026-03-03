#pragma once

struct BackgroundLayerDrawContext
{
    float worldScrollOffset = 0.0f;
    float pixelsPerUnit = 1.0f;
    float viewportWidth = 0.0f;
    float viewportHeight = 0.0f;
    float normalizedScroll01 = 0.0f;
};

class IBackgroundLayerRenderer
{
public:
    virtual ~IBackgroundLayerRenderer() = default;

    virtual void Update(float dt) = 0;
    virtual void Draw(const BackgroundLayerDrawContext& context) = 0;
};
