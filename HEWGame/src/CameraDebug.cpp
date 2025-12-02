#include "CameraDebug.h"
#include "Core/Input.h"
#include "Defines.h"

CameraDebug::CameraDebug(float3 a_fPos, float a_fFovy)
: CameraSpace::BaseCamera(a_fPos)
, Core::UpdateNode(Core::UpdatePhase::Camera)
, bIsMove(true)
{
SetPerspective(a_fFovy,
static_cast<float>(SCREEN_WIDTH) / static_cast<float>(SCREEN_HEIGHT),
CMETER(30.0f),
METER(1000.0f));
}

void CameraDebug::OnUpdate(Core::UpdateContext& ctx)
{
        if (ctx.input.IsTrigger(Core::ActionId::ToggleCameraMove))
        {
                bIsMove = !bIsMove;
        }

        if (bIsMove)
        {
                constexpr float kMoveSpeed = 6.0f;
                float3 position = GetPosition();
                position.y += kMoveSpeed * ctx.dt;
                SetPosition(position);
        }
}
