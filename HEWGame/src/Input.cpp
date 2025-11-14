#include "Core/Input.h"

namespace Core
{
        namespace
        {
                constexpr BYTE kKeyPressedMask = 0x80;
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
                PollKeyboardState();
                m_previousKeys = m_currentKeys;
                m_initialized = true;
                return S_OK;
        }

        void Input::Shutdown()
        {
                m_initialized = false;
                m_currentKeys.fill(0);
                m_previousKeys.fill(0);
        }

        void Input::OnUpdate(UpdateContext& ctx)
        {
                if (!m_initialized)
                {
                        return;
                }

                if (&ctx.input != this)
                {
                        OutputDebugStringA("[Core::Input] UpdateContext input reference mismatch.\n");
                }

                m_previousKeys = m_currentKeys;
                PollKeyboardState();
        }

        bool Input::IsKeyPress(BYTE key) const
        {
                return (m_currentKeys[key] & kKeyPressedMask) != 0;
        }

        bool Input::IsKeyTrigger(BYTE key) const
        {
                return ((m_currentKeys[key] ^ m_previousKeys[key]) & m_currentKeys[key] & kKeyPressedMask) != 0;
        }

        bool Input::IsKeyRelease(BYTE key) const
        {
                return ((m_currentKeys[key] ^ m_previousKeys[key]) & m_previousKeys[key] & kKeyPressedMask) != 0;
        }

        bool Input::IsKeyRepeat(BYTE) const
        {
                return false;
        }

        void Input::PollKeyboardState()
        {
                if (!::GetKeyboardState(m_currentKeys.data()))
                {
                        m_currentKeys.fill(0);
                        OutputDebugStringA("[Core::Input] GetKeyboardState failed; clearing keyboard state.\n");
                }
        }
}
