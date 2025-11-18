// ============================================================================
// 概要: CameraDebug の挙動や関連ロジックを実装し、src に属する機能を具体化している。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: src 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

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
if (ctx.input.IsKeyTrigger(VK_RETURN))
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
