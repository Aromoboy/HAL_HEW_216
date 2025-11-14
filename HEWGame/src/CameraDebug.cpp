#include "CameraDebug.h"
#include "Core/Input.h"

void CameraDebug::Update()
{
        // UpdateNode経由で処理するのでここでは何もしない
}

void CameraDebug::OnUpdate(Core::UpdateContext& ctx)
{
        if (ctx.input.IsKeyTrigger(VK_RETURN))
        {
                bIsMove = !bIsMove;
        }

        if (bIsMove)
        {
                constexpr float kMoveSpeed = 6.0f;
                m_pos.y += kMoveSpeed * ctx.dt;
        }
}
