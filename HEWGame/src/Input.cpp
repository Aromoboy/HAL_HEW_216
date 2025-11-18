// ============================================================================
// 概要: Input の挙動や関連ロジックを実装し、src に属する機能を具体化している。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: src 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

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
