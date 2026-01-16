#pragma once

#include <Windows.h>
#include <array>
#include <cstdint>
#include "Core/ActionId.h"
#include "Core/InputMap.h"
#include "Core/UpdateNode.h"

// Input/UpdateContext 対応メモ
// - 本クラスは Core::UpdateNode を継承し、UpdatePhase::Input で最初に入力を集約する役割を担う 
// - UpdateContext には時間情報のみを保持し、入力参照は各システムから明示的に渡す 
// - ゲームループの更新順序を崩さないため、ポーリングは OnUpdate 内でのみ行い、他フェーズは取得済み状態を参照する方針 

namespace Core
{
    class Input : public UpdateNode
    {
    public:
        Input();

        HRESULT Initialize();
        void Shutdown();

        void OnUpdate(const UpdateContext& ctx) override;

        float GetAxis(ActionId action) const;

        bool IsPress(BYTE key) const;
        bool IsTrigger(BYTE key) const;
        bool IsRelease(BYTE key) const;

        bool IsAction(ActionId action) const;
        bool IsPress(ActionId action) const;
        bool IsTrigger(ActionId action) const;
        bool IsRelease(ActionId action) const;

        bool IsKeyPress(BYTE key) const;
        bool IsKeyTrigger(BYTE key) const;
        bool IsKeyRelease(BYTE key) const;
        bool IsKeyRepeat(BYTE key) const;

        bool IsPress(int vk) const;
        bool IsTrigger(int vk) const;
        bool IsRelease(int vk) const;

        void Rebind(ActionId action, const Binding& binding);

    private:
        // 毎フレーム、入力フェーズ開始時に現在のキー状態を取得する 
        // 初期化に失敗した場合は安全側に倒すためゼロクリアする 
        void PollKeyboardState();

        // 現フレーム/前フレームの状態を参照するヘルパー 未初期化時は false になる 
        InputMap::ActionState GetActionState(ActionId action) const;

        bool IsKeyDown(BYTE key) const;
        bool WasKeyDown(BYTE key) const;

        // 配列は UpdatePhase::Input の開始時に current へ読み出し、更新前に previous に退避する 
        std::array<BYTE, 256> m_currentKeys;
        std::array<BYTE, 256> m_previousKeys;
        bool m_initialized;

        InputMap m_map;
        std::uint64_t m_lastPolledFrame = 0;

        friend class InputMap;
    };
}
