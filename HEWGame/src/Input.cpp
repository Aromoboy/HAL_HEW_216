#include "Core/Input.h"

#include <sstream>

namespace
{
        constexpr BYTE kKeyPressedMask = 0x80;
}

namespace Core
{
        Input::Input()
                : UpdateNode(UpdatePhase::Input, 0)
                , m_currentKeys{}
                , m_previousKeys{}
                , m_initialized(false)
                , m_pad{}
                , m_map{}
        {
        }

        HRESULT Input::Initialize()
        {
                // 初期化順序: 先に現在のキー状態を取得し、そのスナップショットを前フレームとして保存する。
                // これにより、最初のフレームで「前回なし→今回あり」のトリガー判定が行える。
                PollKeyboardState();
                m_previousKeys = m_currentKeys;

                PollPadState();

                m_initialized = true;
                return S_OK;
        }

        void Input::Shutdown()
        {
                // UpdateScheduler から除外される前提だが、保険として状態をリセットして無効化する。
                m_initialized = false;
                m_currentKeys.fill(0);
                m_previousKeys.fill(0);

                m_pad = Gamepad();
                m_map = InputMap();
        }

        void Input::OnUpdate(UpdateContext& ctx)
        {
                if (!m_initialized)
                {
                        // Initialize を忘れた場合は安全に抜ける。入力参照先が未初期化であることがデバッグログから分かるようにする。
                        OutputDebugStringA("[Core::Input] OnUpdate が未初期化状態で呼ばれました。推奨: Initialize 呼び出し順を確認してください。\n");
                        return;
                }

                if (&ctx.input != this)
                {
                        OutputDebugStringA("[Core::Input] UpdateContext.input が別インスタンスを指しています。推奨: Input の生成/登録順を確認してください。\n");
                }

                // 更新順序: 前フレームを退避 → 現在のハードウェア状態を読み取り → マッピング層が参照できるよう保持。
                m_previousKeys = m_currentKeys;
                PollKeyboardState();

                PollPadState();
        }

        float Input::GetAxis(ActionId action) const
        {
                return m_map.GetAxis(action, *this, m_pad);
        }

        bool Input::IsPress(BYTE key) const
        {
                return IsKeyPress(key);
        }

        bool Input::IsTrigger(BYTE key) const
        {
                return IsKeyTrigger(key);
        }

        bool Input::IsRelease(BYTE key) const
        {
                return IsKeyRelease(key);
        }

        bool Input::IsAction(ActionId action) const
        {
                return m_map.IsPressed(action, *this, m_pad);
        }

        bool Input::IsTrigger(ActionId action) const
        {
                const auto state = m_map.GetState(action, *this, m_pad);
                if (state.current && !state.previous)
                {
                        std::ostringstream oss;
                        oss << "[Core::Input] アクション " << static_cast<int>(action)
                                << " が立ち上がりました。推奨: バインド設定や入力遷移の想定と一致するか確認してください。\n";
                        OutputDebugStringA(oss.str().c_str());
                }
                return state.current && !state.previous;
        }

        bool Input::IsRelease(ActionId action) const
        {
                const auto state = m_map.GetState(action, *this, m_pad);
                return !state.current && state.previous;
        }

        void Input::Rebind(ActionId action, const Binding& binding)
        {
                m_map.Rebind(action, binding);
        }

        bool Input::IsKeyPress(BYTE key) const
        {
                return IsKeyDown(key);
        }

        bool Input::IsKeyTrigger(BYTE key) const
        {
                return IsKeyDown(key) && !WasKeyDown(key);
        }

        bool Input::IsKeyRelease(BYTE key) const
        {
                return !IsKeyDown(key) && WasKeyDown(key);
        }

        bool Input::IsKeyRepeat(BYTE key) const
        {
                return IsKeyDown(key);
        }

        bool Input::IsPress(ActionId action) const
        {
                return m_map.IsPressed(action, *this, m_pad);
        }

        bool Input::IsTrigger(int vk) const
        {
                return IsKeyDown(static_cast<BYTE>(vk)) && !WasKeyDown(static_cast<BYTE>(vk));
        }

        bool Input::IsRelease(int vk) const
        {
                return !IsKeyDown(static_cast<BYTE>(vk)) && WasKeyDown(static_cast<BYTE>(vk));
        }

        bool Input::IsPress(int vk) const
        {
                return IsKeyDown(static_cast<BYTE>(vk));
        }

        void Input::PollKeyboardState()
        {
                if (!::GetKeyboardState(m_currentKeys.data()))
                {
                        m_currentKeys.fill(0);
                        OutputDebugStringA("[Core::Input] GetKeyboardState に失敗したためキーボード状態を初期化しました。推奨: フォーカス/アクセス権限を確認してください。\n");
                }
        }

        void Input::PollPadState(int index)
        {
                m_pad.Poll(index);
        }

        bool Input::IsKeyDown(BYTE key) const
        {
                return (m_currentKeys[key] & kKeyPressedMask) != 0;
        }

        bool Input::WasKeyDown(BYTE key) const
        {
                return (m_previousKeys[key] & kKeyPressedMask) != 0;
        }
}

