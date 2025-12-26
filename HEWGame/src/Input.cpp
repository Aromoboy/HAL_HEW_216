#include "Core/Input.h"

#include <sstream>
#include <core/StepTimer.h>
#include "Diagnostics/Logger.h"

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
                // 初期化順序: 先に現在のキー状態を取得し、そのスナップショットを前フレームとして保存する 
                // これにより、最初のフレームで「前回なし→今回あり」のトリガー判定が行える 
                PollKeyboardState();
                m_previousKeys = m_currentKeys;

                PollPadState();

                m_initialized = true;
                return S_OK;
        }

        void Input::Shutdown()
        {
                // UpdateScheduler から除外される前提だが、保険として状態をリセットして無効化する 
                m_initialized = false;
                m_currentKeys.fill(0);
                m_previousKeys.fill(0);

                m_pad = Gamepad();
                m_map = InputMap();
                m_lastPolledFrame = 0;
        }

        void Input::OnUpdate(UpdateContext& ctx)
        {
                const std::uint64_t frameIndex = ctx.timer.FrameIndex();

                if (frameIndex == m_lastPolledFrame)
                {
                        return;
                }

                if (m_initialized && m_lastPolledFrame != 0 && frameIndex > m_lastPolledFrame + 1)
                {
                        std::ostringstream oss;
                        oss << "[Input] polling resumed after skipping "
                            << (frameIndex - m_lastPolledFrame - 1) << " frame(s) (last=" << m_lastPolledFrame
                            << ", current=" << frameIndex << ')';
                        Diagnostics::LogInfo(oss.str());
                }
                if (!m_initialized)
                {
                        std::ostringstream oss;
                        oss << "[Input] initializing poll pipeline at frame " << frameIndex;
                        Diagnostics::LogInfo(oss.str());
                        // OnUpdate 経由での遅延初期化 ポーリング結果を保存し、マッピング層が最新の入力を参照できるようにする 
                        PollKeyboardState();
                        m_previousKeys = m_currentKeys;
                        PollPadState();
                        m_initialized = true;
                }
                else
                {
                        // 更新順序: 前フレームを退避 → 現在のハードウェア状態を読み取り → マッピング層が参照できるよう保持 
                        m_previousKeys = m_currentKeys;
                        PollKeyboardState();
                        PollPadState();
                }

                m_lastPolledFrame = frameIndex;
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
                return IsPress(action);
        }

        bool Input::IsTrigger(ActionId action) const
        {
                return m_map.Is(action, *this, m_pad);
        }

        bool Input::IsRelease(ActionId action) const
        {
                const auto state = GetActionState(action);
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
                return GetActionState(action).current;
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

        InputMap::ActionState Input::GetActionState(ActionId action) const
        {
                return m_map.GetState(action, *this, m_pad);
        }

        void Input::PollKeyboardState()
        {
                if (!::GetKeyboardState(m_currentKeys.data()))
                {
                        m_currentKeys.fill(0);
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
