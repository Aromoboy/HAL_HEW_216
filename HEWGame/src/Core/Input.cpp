#include "Core/Input.h"

#include "Input.h"

namespace Core
{
        Input::Input()
                : m_initialized(false)
        {
        }

        HRESULT Input::Initialize()
        {
                if (m_initialized)
                {
                        return S_OK;
                }

                HRESULT hr = InitInput();
                if (SUCCEEDED(hr))
                {
                        m_initialized = true;
                }
                return hr;
        }

        void Input::Shutdown()
        {
                if (!m_initialized)
                {
                        return;
                }

                UninitInput();
                m_initialized = false;
        }

        void Input::Update()
        {
                UpdateInput();
        }

        bool Input::IsKeyPress(std::uint8_t key) const
        {
                return ::IsKeyPress(key);
        }

        bool Input::IsKeyTrigger(std::uint8_t key) const
        {
                return ::IsKeyTrigger(key);
        }

        bool Input::IsKeyRelease(std::uint8_t key) const
        {
                return ::IsKeyRelease(key);
        }

        bool Input::IsKeyRepeat(std::uint8_t key) const
        {
                return ::IsKeyRepeat(key);
        }
}

