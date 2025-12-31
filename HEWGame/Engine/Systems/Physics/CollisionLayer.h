// コライダーの接触判定で使う層分けを定義する
// 物理登録や接触判定が参照する

#pragma once

namespace Collision
{
    enum class CollisionLayer
    {
        Default,
        Player,
        Floor,
        Stage,
        Sweets,
    };
}
