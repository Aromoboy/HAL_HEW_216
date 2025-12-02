#include "Core/Input.h"

#include <sstream>

namespace
{
        constexpr BYTE kKeyPressedMask = 0x80;
        constexpr float kStickNorm = 32767.0f;
}

namespace Core
{
        Gamepad::Gamepad()
                : m_state{}
                , m_prevState{}
                , m_connected(false)
        {
        }

        void Gamepad::Poll(int index)
        {
                m_prevState = m_state;
                const bool wasConnected = m_connected;

                XINPUT_STATE state{};
                DWORD result = XInputGetState(index, &state);

                if (result == ERROR_SUCCESS)
                {
                        m_connected = true;
                        m_state = state;
                }
                else
                {
                        m_connected = false;
                        ZeroMemory(&m_state, sizeof(XINPUT_STATE));
                }

                if (m_connected != wasConnected)
                {
                        OutputDebugStringA(m_connected
                                ? "[Core::Gamepad] コントローラを検出しました（状態を更新）。推奨: そのまま入力テストを継続してください。\n"
                                : "[Core::Gamepad] コントローラが切断されました（状態を初期化）。推奨: ケーブル/電源を確認し、再接続後に挙動を確認してください。\n");
                }
        }

        bool Gamepad::IsConnected() const
        {
                return m_connected;
        }

        bool Gamepad::IsButtonDown(WORD button) const
        {
                return m_connected && (m_state.Gamepad.wButtons & button) != 0;
        }

        bool Gamepad::WasButtonDown(WORD button) const
        {
                return m_connected && (m_prevState.Gamepad.wButtons & button) != 0;
        }

        float Gamepad::AxisLX() const { return m_connected ? Normalize(m_state.Gamepad.sThumbLX) : 0.0f; }
        float Gamepad::AxisLY() const { return m_connected ? Normalize(m_state.Gamepad.sThumbLY) : 0.0f; }
        float Gamepad::AxisRX() const { return m_connected ? Normalize(m_state.Gamepad.sThumbRX) : 0.0f; }
        float Gamepad::AxisRY() const { return m_connected ? Normalize(m_state.Gamepad.sThumbRY) : 0.0f; }

        float Gamepad::Normalize(short value) const
        {
                return static_cast<float>(value) / kStickNorm;
        }

        Input::Input()
                : UpdateNode(UpdatePhase::Input, 0)
                , m_currentKeys{}
                , m_previousKeys{}
                , m_initialized(false)
        {
        }

        HRESULT Input::Initialize()
        {
                // 初期化順序: 先に現在のキー状態を取得し、そのスナップショットを前フレームとして保存する。
                // これにより、最初のフレームで「前回なし→今回あり」のトリガー判定が行える。
                PollKeyboardState();
                m_previousKeys = m_currentKeys;

                // パッドも同様に初回の状態を確定させる。未接続時はゼロクリアされた状態が入る。
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
        }

        void Input::OnUpdate(UpdateContext& ctx)
        {
                if (!m_initialized)
                {
                        // Initialize を忘れた場合は安全に抜ける。入力参照先が未初期化であることがデバッグログから分かるようにする。
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

        bool Input::IsPress(ActionId action) const
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

        bool Input::IsAction(ActionId action) const
        {
                return IsTrigger(action);
        }

        void Input::Rebind(ActionId action, const Binding& binding)
        {
                m_map.Rebind(action, binding);
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

        bool Input::IsPress(int vk) const
        {
                return IsKeyDown(static_cast<BYTE>(vk));
        }

        bool Input::IsTrigger(int vk) const
        {
                return IsKeyDown(static_cast<BYTE>(vk)) && !WasKeyDown(static_cast<BYTE>(vk));
        }

        bool Input::IsRelease(int vk) const
        {
                return !IsKeyDown(static_cast<BYTE>(vk)) && WasKeyDown(static_cast<BYTE>(vk));
        }
}

