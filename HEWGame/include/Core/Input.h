// ============================================================================
// 概要: Input に関するインターフェースやデータ構造を宣言し、include／Core から再利用できるようにする。
// 目的: 初めてこのモジュールに触れる開発者でも、責務と依存関係を素早く把握できるように高レベルの説明を付与している。
// 連携: include／Core 配下の他ファイルやシステム層と連動するため、ファイル間の関係を追跡する際の目印として利用できる。
// ============================================================================

#pragma once

#include <Windows.h>
#include <array>

#include "Core/UpdateNode.h"

namespace Core
{
        class Input : public UpdateNode
        {
        public:
                Input();

                HRESULT Initialize();
                void Shutdown();

                void OnUpdate(UpdateContext& ctx) override;

                bool IsKeyPress(BYTE key) const;
                bool IsKeyTrigger(BYTE key) const;
                bool IsKeyRelease(BYTE key) const;
                bool IsKeyRepeat(BYTE key) const;

        private:
                void PollKeyboardState();

                std::array<BYTE, 256> m_currentKeys;
                std::array<BYTE, 256> m_previousKeys;
                bool m_initialized;
        };
}

