from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUTPUT = ROOT / "build/mika_october/frame_pacing"
PRELUDE = r'''
#include <windows.h>
#include <mmsystem.h>
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cassert>
#pragma comment(lib, "winmm.lib")
using Clock = std::chrono::steady_clock;
constexpr int FPS = 60;
constexpr float fFPS = 60.0f;
bool g_canRender = true;
std::vector<Clock::time_point> frames;
void Update() {
    frames.push_back(Clock::now());
    const auto end = Clock::now() + std::chrono::milliseconds(2);
    while (Clock::now() < end) {}
}
void Draw() {}
bool IsEnd() { return false; }
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_APP) { g_canRender = false; return 0; }
    if (message == WM_APP + 1) { g_canRender = true; return 0; }
    return DefWindowProc(window, message, w, l);
}
double ThreadMilliseconds() {
    FILETIME create, exit, kernel, user;
    GetThreadTimes(GetCurrentThread(), &create, &exit, &kernel, &user);
    ULARGE_INTEGER k, u;
    k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime;
    return (k.QuadPart + u.QuadPart) / 10000.0;
}
void Pump(HWND hWnd) {
    MSG message{};
'''
POSTLUDE = r'''
}
int main() {
    WNDCLASS wc{}; wc.lpfnWndProc = WindowProc; wc.lpszClassName = L"MikaPacingRegression";
    wc.hInstance = GetModuleHandle(nullptr);
    assert(RegisterClass(&wc));
    HWND window = CreateWindow(wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    assert(window);
    for (int mode = 0; mode < 3; ++mode) {
        frames.clear(); g_canRender = mode != 1;
        DWORD thread = GetCurrentThreadId();
        std::thread stop([=] {
            if (mode == 2) {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                PostMessage(window, WM_APP, 0, 0);
                std::this_thread::sleep_for(std::chrono::milliseconds(900));
                PostMessage(window, WM_APP + 1, 0, 0);
                std::this_thread::sleep_for(std::chrono::milliseconds(800));
            } else std::this_thread::sleep_for(std::chrono::seconds(2));
            PostThreadMessage(thread, WM_QUIT, 0, 0);
        });
        const double cpuBefore = ThreadMilliseconds();
        auto start = Clock::now();
        Pump(window);
        const double wall = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        const double cpu = ThreadMilliseconds() - cpuBefore;
        stop.join();
        std::vector<double> intervals;
        for (size_t i = 1; i < frames.size(); ++i)
            intervals.push_back(std::chrono::duration<double, std::milli>(frames[i] - frames[i - 1]).count());
        std::sort(intervals.begin(), intervals.end());
        if (mode == 1) assert(frames.empty());
        else assert(frames.size() > 30 && frames.size() < 140);
        if (mode == 2) assert(intervals.back() > 800);
        std::printf("%s frames=%zu wall_ms=%.2f cpu_ms=%.2f core_percent=%.2f p95_interval_ms=%.3f\n",
            mode == 0 ? "active_2ms_work" : mode == 1 ? "inactive" : "pause_resume",
            frames.size(), wall, cpu, cpu / wall * 100,
            intervals.empty() ? 0 : intervals[intervals.size() * 95 / 100]);
    }
    DestroyWindow(window);
    std::puts("PASS: message wake, pause, resume, shutdown, bounded frame rate");
}
'''


def main():
    # ミカ：保存した変更前と現在の実際のループを同じ入力と2msの処理で比較する
    OUTPUT.mkdir(parents=True, exist_ok=True)
    vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
    vs = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires",
                                  "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                                  "-property", "installationPath"], text=True).strip()
    vcvars = Path(vs) / "VC/Auxiliary/Build/vcvars64.bat"
    sources = {"after": ROOT / "HEWGame/Game/Scene/Startup.cpp"}
    before = ROOT / "build/mika_october/optimization-before/HEWGame/Game/Scene/Startup.cpp"
    if before.exists():
        sources = {"before": before, **sources}
    for label, path in sources.items():
        source = path.read_text(encoding="utf-8-sig")
        begin = source.index("timeBeginPeriod(1);")
        end = source.index("timeEndPeriod(1);", begin) + len("timeEndPeriod(1);")
        (OUTPUT / f"{label}.cpp").write_text(PRELUDE + source[begin:end] + POSTLUDE, encoding="utf-8")
        (OUTPUT / "compile.cmd").write_text(
            f'@call "{vcvars}" >nul\n@cl /nologo /std:c++20 /O2 /MT /EHsc /utf-8 /DNOMINMAX /DUNICODE /D_UNICODE {label}.cpp /Fe:{label}.exe user32.lib\n',
            encoding="utf-8")
        subprocess.run(["cmd", "/d", "/c", "compile.cmd"], cwd=OUTPUT, check=True)
        run = subprocess.check_output([str(OUTPUT / f"{label}.exe")], text=True)
        (OUTPUT / f"{label}.log").write_text(run, encoding="utf-8")
        print(label + "\n" + run)


if __name__ == "__main__":
    main()
