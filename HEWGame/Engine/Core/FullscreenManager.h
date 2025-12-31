// 画面の全画面切り替えを扱う
// ウィンドウ制御や設定画面から呼ばれる

#pragma once

#include <Windows.h>

namespace FullscreenManager
{
    // ウィンドウハンドルとレンダリング可用性フラグへのポインタで初期化
    // このフラグはアプリケーションの実行期間中、有効な状態を維持する必要
    void Initialize(HWND hWnd, bool* canRenderFlag);

    // トグル操作が安全かどうかを返す（ウィンドウが存在し、最小化されておらず、描画可能であること）
    bool CanToggle();

    // キャッシュされたウィンドウ状態とレンダリングの可用性を尊重しながら、全画面表示を切り替える
    void Toggle();
}

