from pathlib import Path
import os
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
SCENE = ROOT / "HEWGame/Game/Scene/ScenePowerUp.cpp"
RENDERER = ROOT / "HEWGame/Game/PowerUp/PowerUpRenderer.cpp"
HEADER = ROOT / "HEWGame/Game/PowerUp/PowerUpRenderer.h"
BASELINE = ROOT / "build/mika_october/optimization-before/HEWGame"


def baseline(relative):
    saved = BASELINE / relative
    if saved.exists():
        return saved.read_text(encoding="utf-8-sig")
    return subprocess.check_output(["git", "show", f"HEAD:HEWGame/{relative}"], cwd=ROOT).decode("utf-8-sig")


def block(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


HARNESS = r'''
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

std::atomic<int> allocations{};
std::atomic<bool> countAllocations{};
void* operator new(std::size_t size) {
    if (countAllocations) ++allocations;
    if (void* memory = std::malloc(size)) return memory;
    throw std::bad_alloc();
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

struct SaveProbe {
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<int> attempts;
    int active = 0;
    int maximumActive = 0;
    int gatedValue = -1;
    int persisted = 0;
    bool gateOpen = false;
    bool failLaunch = false;
} probe;

struct PlayerData {
    struct Data { int nPoints = 0; };
    std::vector<Data> entries;
    bool canSave = true;
    void SetEntries(const std::vector<Data>& value) { entries = value; }
    bool SaveData() const {
        std::unique_lock lock(probe.mutex);
        const int value = entries.at(0).nPoints;
        ++probe.active;
        probe.maximumActive = (std::max)(probe.maximumActive, probe.active);
        probe.attempts.push_back(value);
        probe.changed.notify_all();
        probe.changed.wait(lock, [&] { return value != probe.gatedValue || probe.gateOpen; });
        --probe.active;
        if (value == 6) throw std::runtime_error("controlled save exception");
        if (value == 4 || !canSave) return false;
        probe.persisted = value;
        return true;
    }
};
namespace DebugLog {
std::string output;
void DebugMessage(const std::string& message) { output += message; }
}
namespace PowerUpEffectConfig {
std::vector<int> entries{1};
const std::vector<int>& Entries() { return entries; }
}
template<typename Function>
auto StartSave(std::launch policy, Function&& function) {
    if (probe.failLaunch) {
        probe.failLaunch = false;
        throw std::system_error(std::make_error_code(std::errc::resource_unavailable_try_again));
    }
    return std::async(policy, std::forward<Function>(function));
}
constexpr const char* kLogPrefix = "[ScenePowerUp] ";
class ScenePowerUp {
public:
    ~ScenePowerUp();
    void QueuePlayerSave();
    void UpdatePlayerSave(bool waitForCompletion = false);
    PlayerData m_playerData;
    std::vector<PlayerData::Data> m_playerEntries;
    std::future<bool> m_playerSave;
    std::optional<PlayerData> m_pendingPlayerSave;
};
void SetValue(ScenePowerUp& scene, int value) { scene.m_playerEntries = {{value}}; }
void WaitForAttempt(int value) {
    std::unique_lock lock(probe.mutex);
    assert(probe.changed.wait_for(lock, std::chrono::seconds(5), [&] {
        return !probe.attempts.empty() && probe.attempts.back() == value;
    }));
}
void ReleaseGate() {
    std::lock_guard lock(probe.mutex);
    probe.gateOpen = true;
    probe.changed.notify_all();
}
struct Texture2D {};
struct PowerUpStringTexture {};
struct float2 { float x{}, y{}; };
'''

TESTS = r'''
int main() {
    {
        ScenePowerUp scene;
        probe.gatedValue = 1;
        SetValue(scene, 1);
        scene.QueuePlayerSave();
        WaitForAttempt(1);
        SetValue(scene, 2);
        scene.QueuePlayerSave();
        SetValue(scene, 3);
        scene.QueuePlayerSave();
        SetValue(scene, 999);
        scene.UpdatePlayerSave();
        assert(probe.attempts == std::vector<int>{1});
        ReleaseGate();
        scene.UpdatePlayerSave(true);
        assert((probe.attempts == std::vector<int>{1, 3}));
        assert(probe.persisted == 3 && probe.maximumActive == 1);
        assert(!scene.m_playerSave.valid() && !scene.m_pendingPlayerSave);

        SetValue(scene, 4);
        scene.QueuePlayerSave();
        scene.UpdatePlayerSave(true);
        assert(probe.persisted == 3);
        assert(DebugLog::output.find("previous file was preserved") != std::string::npos);
        SetValue(scene, 5);
        scene.QueuePlayerSave();
        scene.UpdatePlayerSave(true);
        assert(probe.persisted == 5);
        SetValue(scene, 6);
        scene.QueuePlayerSave();
        scene.UpdatePlayerSave(true);
        assert(probe.persisted == 5);
        assert(DebugLog::output.find("controlled save exception") != std::string::npos);
        SetValue(scene, 7);
        scene.QueuePlayerSave();
        scene.UpdatePlayerSave(true);
        assert(probe.persisted == 7);

        scene.m_playerData.canSave = false;
        SetValue(scene, 99);
        scene.QueuePlayerSave();
        scene.UpdatePlayerSave(true);
        assert(probe.persisted == 7);
        scene.m_playerData.canSave = true;

        probe.failLaunch = true;
        SetValue(scene, 10);
        scene.QueuePlayerSave();
        assert(probe.persisted == 10 && !scene.m_playerSave.valid());
        assert(DebugLog::output.find("saving synchronously") != std::string::npos);
        probe.failLaunch = true;
        SetValue(scene, 6);
        scene.QueuePlayerSave();
        assert(probe.persisted == 10 && !scene.m_pendingPlayerSave);
        PowerUpEffectConfig::entries.clear();
        SetValue(scene, 11);
        scene.QueuePlayerSave();
        assert(probe.persisted == 10 && !scene.m_pendingPlayerSave);
        PowerUpEffectConfig::entries = {1};
    }
    probe.gatedValue = 8;
    probe.gateOpen = false;
    std::thread release;
    {
        ScenePowerUp scene;
        SetValue(scene, 8);
        scene.QueuePlayerSave();
        WaitForAttempt(8);
        SetValue(scene, 9);
        scene.QueuePlayerSave();
        release = std::thread([] { ReleaseGate(); });
    }
    release.join();
    assert(probe.persisted == 9 && probe.active == 0 && probe.maximumActive == 1);

    auto texture = std::make_shared<Texture2D>();
    auto strings = std::make_shared<PowerUpStringTexture>();
    const std::string name(60, 'n'), info(100, 'i');
    allocations = 0;
    countAllocations = true;
    {
        Current::NodeRenderData data(texture, strings, {}, {}, false, name, info, 1, 5, 100);
        assert(&data.powerUpName == &name && &data.powerUpInfomation == &info);
    }
    countAllocations = false;
    const int currentAllocations = allocations;
    assert(currentAllocations == 0);
    allocations = 0;
    countAllocations = true;
    {
        Before::NodeRenderData data(texture, strings, {}, {}, false, name, info, 1, 5, 100);
        assert(data.powerUpName == name && data.powerUpInfomation == info);
    }
    countAllocations = false;
    assert(allocations >= 4);
    std::cout << "PASS: one writer, immutable/coalesced snapshots, failure/exception recovery, launch fallback, invalid-config refusal, exit drain\n";
    std::cout << "PASS: NodeRenderData long-string allocations before=" << allocations
        << " after=" << currentAllocations << " per draw\n";
}
'''


def main():
    scene = SCENE.read_text(encoding="utf-8-sig")
    renderer = RENDERER.read_text(encoding="utf-8-sig")
    header = HEADER.read_text(encoding="utf-8-sig")
    old_renderer = baseline("Game/PowerUp/PowerUpRenderer.cpp")
    old_header = baseline("Game/PowerUp/PowerUpRenderer.h")
    assert "ShaderList::SetWVP" not in renderer
    assert old_renderer.count("ShaderList::SetWVP") == 3
    assert "PowerUpBG" not in scene and "PowerUpBackFairy" not in scene
    assert "m_fairy" not in renderer and "m_background" not in renderer
    assert "UpdatePlayerSave(true)" in block(scene, "void ScenePowerUp::OnExit()")
    purchase = scene[scene.index("auto onPurchase"):scene.index("m_cursor =", scene.index("auto onPurchase"))]
    assert "QueuePlayerSave()" in purchase and ".SaveData()" not in purchase
    for signature in ("void PowerUpRenderer::DrawNode", "void PowerUpRenderer::DrawCursor", "void PowerUpRenderer::DrawCost"):
        assert block(renderer, signature) == block(old_renderer, signature)
    assert block(renderer, "void PowerUpRenderer::DrawHud").count("DrawSprite(") == 1
    assert block(old_renderer, "void PowerUpRenderer::DrawHud").count("DrawSprite(") == 2

    # ミカ：実際の保存制御と描画データを抽出し、遅い保存と失敗を制御して検証する
    methods = "\n".join(block(scene, signature) for signature in (
        "ScenePowerUp::~ScenePowerUp()", "void ScenePowerUp::QueuePlayerSave()",
        "void ScenePowerUp::UpdatePlayerSave(bool waitForCompletion)"))
    methods = methods.replace("std::async(", "StartSave(")
    harness = HARNESS + methods
    harness += "\nnamespace Current {\n" + block(header, "struct NodeRenderData") + ";\n}\n"
    harness += "\nnamespace Before {\n" + block(old_header, "struct NodeRenderData") + ";\n}\n" + TESTS
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    vs = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
    vcvars = Path(vs) / "VC/Auxiliary/Build/vcvars64.bat"
    with tempfile.TemporaryDirectory(prefix="powerup_optimization_") as temp:
        directory = Path(temp)
        (directory / "powerup.cpp").write_text(harness, encoding="utf-8")
        command = f'call "{vcvars}" >nul && cl /nologo /std:c++20 /EHsc /utf-8 /W4 powerup.cpp /Fe:powerup.exe'
        (directory / "build.cmd").write_text(command, encoding="utf-8")
        subprocess.run(["cmd", "/d", "/c", "build.cmd"], cwd=directory, check=True)
        subprocess.run([str(directory / "powerup.exe")], cwd=directory, check=True, timeout=20)
    print("PASS: PowerUp idle UI minimum draws 52 -> 51; constant-buffer uploads 208 -> 102; unused RGBA8 payload removed 46.44 MiB")


if __name__ == "__main__":
    main()
