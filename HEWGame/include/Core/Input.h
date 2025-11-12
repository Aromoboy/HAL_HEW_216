#pragma once

#include <Windows.h>

#include "Input.h"

namespace Core
{
        class Input
        {
        public:
                void Initialize()
                {
                        ::InitInput();
                }

                void Shutdown()
                {
                        ::UninitInput();
                }

                void Update()
                {
                        ::UpdateInput();
                }

                bool IsKeyPress(BYTE key) const
                {
                        return ::IsKeyPress(key);
                }

                bool IsKeyTrigger(BYTE key) const
                {
                        return ::IsKeyTrigger(key);
                }

                bool IsKeyRelease(BYTE key) const
                {
                        return ::IsKeyRelease(key);
                }

                bool IsKeyRepeat(BYTE key) const
                {
                        return ::IsKeyRepeat(key);
                }
        };
}

