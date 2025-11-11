#include "CameraDebug.h"
#include "Input.h"

void CameraDebug::Update()
{
        if(IsKeyTrigger(VK_RETURN)) bIsMove ^= 1;

        if(bIsMove) m_pos.y += 0.1f;
}

void CameraDebug::OnUpdate(const Core::UpdateContext& ctx)
{
        (void)ctx;
        Update();
}
