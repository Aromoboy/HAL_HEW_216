from pathlib import Path
import os
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]


def method(path, signature):
    source = (ROOT / path).read_text(encoding="utf-8-sig")
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


# ミカ：生成先だけを簡略化し本番のスケジューラーと入力処理を検証する
SOURCE = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include "Core/Gamepad.h"
#include "Core/InputMap.h"
#include "Core/Input.h"
XINPUT_STATE testPadState{};
DWORD testPadResult = ERROR_SUCCESS;
DWORD TestXInputGetState(DWORD, XINPUT_STATE* state) {
    *state = testPadState;
    return testPadResult;
}
#define XInputGetState TestXInputGetState
#include "Core/Gamepad.cpp"
#undef XInputGetState
#include "Core/InputMap.cpp"

constexpr float kMinimumSpawnIntervalSeconds = 0.1f, kMinimumBurstIntervalSeconds = 0.05f;
constexpr size_t kMaxSpawnQueueEntries = 16;
constexpr const char* kLogPrefix = "[Sweets] ";
void DebugTrace(const std::string&) {}
std::string NormalizeKey(const std::string& value) { return value; }
struct Transform {};
struct Factory { std::string ResolveKindKey(const std::string& value) { return value; } };
class SweetsSpawner {
public:
    enum class SpawnBlockReason { NoWeights, CoolingDown, BlockedByLiveCap, None, NoOrigins, BlockedByPerPointCap };
    struct Origin { const char* laneLabel = "primary"; int index = 0; };
    struct Snapshot {};
    struct Config {
        std::vector<int> weights{1};
        float spawnIntervalSeconds = 2, burstIntervalSeconds = 0.5f;
        int simultaneousSpawnCount = 5, simultaneousLiveTarget = 8;
    } m_spawnTuningConfig;
    int live = 12;
    int m_liveSweetsCap = 15, m_lastLiveSweetsCount = 12;
    int m_spawnCountBonus = 0, m_simultaneousLiveTarget = 8, m_batchStackTarget = 5;
    int m_stackedInBatch = 0, m_activeBurstPlanned = 0, m_currentBurstRemaining = 0, m_pendingTopUpCount = 0;
    bool m_spawnPausedDueToLiveSweets = false, m_batchCoolingDown = false, m_batchInitialSpawnDone = false;
    bool originAvailable = true;
    float m_spawnDebugClockSeconds = 0, m_spawnIntervalMultiplier = 1, m_activeBurstInterval = 0.5f;
    float m_spawnTimer = 0, m_burstTimer = 0.5f;
    std::deque<std::string> m_spawnQueue;
    std::vector<int> m_spawnOriginsPrimary{1,2}, m_spawnOriginsSecondary{1,2};
    Factory* m_factory = nullptr;
    std::function<std::shared_ptr<int>(const std::string&, Transform)> m_spawnCallback;
    SweetsSpawner() { m_spawnCallback = [this](const auto&, auto) { ++live; return std::make_shared<int>(live); }; }
    int CountLiveSweets() const { return live; }
    void ApplyDefaultSpawnConfig() {}
    Snapshot BuildDebugSnapshot(int, int, SpawnBlockReason) const { return {}; }
    void LogBlockReasonTransition(SpawnBlockReason, const Snapshot&) {}
    void LogQueueTransition(size_t, size_t, const Snapshot&) {}
    void LogSpawnDebugEvent(const std::string&, std::string_view) {}
    std::optional<Origin> NextSpawnOrigin() { return originAvailable ? std::optional{Origin{}} : std::nullopt; }
    Transform BuildSpawnTransform(const std::string&, const Origin&) { return {}; }
    void RegisterSpawnOrigin(const std::shared_ptr<int>&, const Origin&) {}
    std::string ChooseKindFromActiveConfig() { return "Strawberry"; }
    void RunSpawnScheduler(float deltaSeconds);
    void EmitSpawnRequests(float deltaSeconds);
    int ComputeDesiredLiveTarget(int remainingToStack) const;
    int EnqueueSpawns(int count);
};
'''

for signature in (
    "void SweetsSpawner::RunSpawnScheduler(",
    "void SweetsSpawner::EmitSpawnRequests(",
    "int SweetsSpawner::ComputeDesiredLiveTarget(",
    "int SweetsSpawner::EnqueueSpawns(",
):
    SOURCE += method("HEWGame/Game/Sweets/SweetsSpawner.cpp", signature) + "\n"

SOURCE += r'''
void CheckSpawnCap() {
    SweetsSpawner normal;
    for (int frame = 0; frame < 200; ++frame) normal.RunSpawnScheduler(1.0f / 60.0f);
    assert(normal.live == 15 && normal.m_spawnQueue.size() == 2);

    for (const float dt : {1.5f, 100.0f}) {
        SweetsSpawner hitch;
        hitch.RunSpawnScheduler(dt);
        assert(hitch.live == 15 && hitch.m_spawnQueue.size() == 2);
        assert(hitch.m_currentBurstRemaining == 2);
        hitch.RunSpawnScheduler(10.0f);
        assert(hitch.live == 15 && hitch.m_spawnQueue.size() == 2);
        --hitch.live;
        hitch.RunSpawnScheduler(1.0f / 60.0f);
        assert(hitch.live == 15 && hitch.m_spawnQueue.size() == 1);
        assert(hitch.m_currentBurstRemaining == 1);
    }

    SweetsSpawner alreadyFull;
    alreadyFull.live = 16;
    alreadyFull.EnqueueSpawns(3);
    alreadyFull.EmitSpawnRequests(10.0f);
    assert(alreadyFull.live == 16 && alreadyFull.m_spawnQueue.size() == 3);
    assert(alreadyFull.m_currentBurstRemaining == 3);

    SweetsSpawner unlimited;
    unlimited.m_liveSweetsCap = 0;
    unlimited.RunSpawnScheduler(100.0f);
    assert(unlimited.live == 17 && unlimited.m_spawnQueue.empty());

    SweetsSpawner failedSpawn;
    int attempts = 0;
    failedSpawn.m_spawnCallback = [&](const auto&, auto) -> std::shared_ptr<int> {
        if (++attempts == 1) return nullptr;
        ++failedSpawn.live;
        return std::make_shared<int>(failedSpawn.live);
    };
    failedSpawn.RunSpawnScheduler(100.0f);
    assert(attempts == 4 && failedSpawn.live == 15);
    assert(failedSpawn.m_spawnQueue.size() == 1 && failedSpawn.m_currentBurstRemaining == 1);

    SweetsSpawner blockedOrigin;
    blockedOrigin.originAvailable = false;
    blockedOrigin.RunSpawnScheduler(100.0f);
    assert(blockedOrigin.live == 12 && blockedOrigin.m_spawnQueue.size() == 5);
    blockedOrigin.originAvailable = true;
    blockedOrigin.RunSpawnScheduler(1.0f / 60.0f);
    assert(blockedOrigin.live == 15 && blockedOrigin.m_spawnQueue.size() == 2);

    SweetsSpawner noCallback;
    noCallback.m_spawnCallback = {};
    noCallback.RunSpawnScheduler(100.0f);
    assert(noCallback.live == 12 && noCallback.m_spawnQueue.size() == 5);
}

void SetAllSticks(short value) {
    testPadState.Gamepad.sThumbLX = value;
    testPadState.Gamepad.sThumbLY = value;
    testPadState.Gamepad.sThumbRX = value;
    testPadState.Gamepad.sThumbRY = value;
}

void CheckStickRange() {
    for (const int deadZone : {XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE}) {
        float previous = -1.0f;
        for (int raw = -32768; raw <= 32767; ++raw) {
            const float axis = Core::Gamepad::NormalizeStickAxis(static_cast<short>(raw), deadZone);
            assert(std::isfinite(axis) && axis >= -1.0f && axis <= 1.0f);
            assert(axis >= previous);
            if (std::abs(raw) <= deadZone) assert(axis == 0.0f);
            else assert((raw < 0) == (axis < 0.0f) && axis != 0.0f);
            previous = axis;
        }
        assert(Core::Gamepad::NormalizeStickAxis(-32768, deadZone) == -1.0f);
        assert(Core::Gamepad::NormalizeStickAxis(32767, deadZone) == 1.0f);
    }

    Core::Gamepad pad;
    SetAllSticks(1024);
    pad.Poll();
    assert(pad.AxisLX() == 0.0f && pad.AxisLY() == 0.0f && pad.AxisRX() == 0.0f && pad.AxisRY() == 0.0f);
    SetAllSticks(8000);
    pad.Poll();
    assert(pad.AxisLX() > 0.0f && pad.AxisLY() > 0.0f);
    assert(pad.AxisRX() == 0.0f && pad.AxisRY() == 0.0f);
    SetAllSticks(-32768);
    pad.Poll();
    assert(pad.AxisLX() == -1.0f && pad.AxisLY() == -1.0f && pad.AxisRX() == -1.0f && pad.AxisRY() == -1.0f);
    SetAllSticks(32767);
    pad.Poll();
    assert(pad.AxisLX() == 1.0f && pad.AxisLY() == 1.0f && pad.AxisRX() == 1.0f && pad.AxisRY() == 1.0f);
}

void CheckInputHistory() {
    Core::Input input;
    for (int axis = 0; axis < 4; ++axis) {
        Core::Gamepad pad;
        Core::InputMap mapping;
        mapping.Rebind(Core::ActionId::MoveX, Core::Binding{0, 0, axis, 1.0f});
        SetAllSticks(10000);
        pad.Poll();
        auto state = mapping.GetState(Core::ActionId::MoveX, input, pad);
        assert(!state.current && !state.previous);
        SetAllSticks(15000);
        pad.Poll();
        state = mapping.GetState(Core::ActionId::MoveX, input, pad);
        assert(state.current && !state.previous);
        assert(mapping.Is(Core::ActionId::MoveX, input, pad));
        pad.Poll();
        state = mapping.GetState(Core::ActionId::MoveX, input, pad);
        assert(state.current && state.previous);
        assert(!mapping.Is(Core::ActionId::MoveX, input, pad));
        SetAllSticks(1024);
        pad.Poll();
        state = mapping.GetState(Core::ActionId::MoveX, input, pad);
        assert(!state.current && state.previous);
        assert(mapping.GetAxis(Core::ActionId::MoveX, input, pad) == 0.0f);
        pad.Poll();
        state = mapping.GetState(Core::ActionId::MoveX, input, pad);
        assert(!state.current && !state.previous);

        mapping.Rebind(Core::ActionId::MoveX, Core::Binding{0, 0, axis, -1.0f});
        SetAllSticks(32767);
        pad.Poll();
        assert(mapping.GetAxis(Core::ActionId::MoveX, input, pad) == -1.0f);
        testPadResult = ERROR_DEVICE_NOT_CONNECTED;
        pad.Poll();
        state = mapping.GetState(Core::ActionId::MoveX, input, pad);
        assert(!pad.IsConnected() && !state.current && state.previous);
        assert(mapping.GetAxis(Core::ActionId::MoveX, input, pad) == 0.0f);
        pad.Poll();
        assert(!mapping.GetState(Core::ActionId::MoveX, input, pad).previous);
        testPadResult = ERROR_SUCCESS;
    }
    Core::Gamepad pad;
    Core::InputMap mapping;
    SetAllSticks(1024);
    pad.Poll();
    input.current['D'] = true;
    assert(mapping.GetAxis(Core::ActionId::MoveX, input, pad) == 1.0f);
}

int main() {
    CheckSpawnCap();
    CheckStickRange();
    CheckInputHistory();
    std::cout << "PASS: spawn cap across hitches, failed/blocked emissions, retained queue, unlimited mode\n"
              << "PASS: all short stick values, SDK deadzones, axis history, disconnect, keyboard\n";
}
'''

INPUT_STUB = r'''
#pragma once
#include <array>
namespace Core {
class Input {
public:
    std::array<bool, 256> current{}, previous{};
    bool IsPress(int key) const { return current.at(key); }
    bool IsKeyDown(unsigned char key) const { return current.at(key); }
    bool WasKeyDown(unsigned char key) const { return previous.at(key); }
};
}
'''

vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
vs = subprocess.check_output([
    str(vswhere), "-latest", "-products", "*", "-requires",
    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath",
], text=True).strip()
vcvars = Path(vs) / "VC/Auxiliary/Build/vcvars64.bat"
with tempfile.TemporaryDirectory(prefix="hew_spawn_input_") as directory:
    out = Path(directory)
    (out / "Core").mkdir()
    (out / "Core/Input.h").write_text(INPUT_STUB, encoding="utf-8")
    (out / "regression.cpp").write_text(SOURCE, encoding="utf-8")
    (out / "build.cmd").write_text(
        f'call "{vcvars}" >nul && cl /nologo /std:c++20 /EHsc /utf-8 /W4 /DNOMINMAX '
        f'/I"{out}" /I"{ROOT / "HEWGame/Engine"}" regression.cpp /Fe:regression.exe',
        encoding="utf-8",
    )
    subprocess.run(["cmd", "/d", "/c", "build.cmd"], cwd=out, check=True)
    subprocess.run([str(out / "regression.exe")], cwd=out, check=True)
