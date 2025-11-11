#pragma once

#include <cstdint>
#include <Windows.h>
#undef max
#undef min

namespace Core
{
        class Input
        {
        public:
                Input();

                HRESULT Initialize();
                void Shutdown();
                void Update();

                bool IsKeyPress(std::uint8_t key) const;
                bool IsKeyTrigger(std::uint8_t key) const;
                bool IsKeyRelease(std::uint8_t key) const;
                bool IsKeyRepeat(std::uint8_t key) const;

        private:
                bool m_initialized;
        };
}

