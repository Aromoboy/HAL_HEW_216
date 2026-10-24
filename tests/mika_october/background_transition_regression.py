from pathlib import Path
import os
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "HEWGame/Game/UI/BackgroundRenderer.cpp"
HEADER = ROOT / "HEWGame/Game/UI/BackgroundRenderer.h"


def block(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def declaration(source, name):
    matches = re.findall(
        rf"^\s*((?:std::array<float,\s*4>|double|float)\s+{name}\b[^;]*;)",
        source,
        flags=re.MULTILINE,
    )
    assert len(matches) == 1, (name, matches)
    return matches[0]


PRELUDE = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#ifdef NDEBUG
#error Assertions must be enabled
#endif
'''

TESTS = r'''
void Near(double actual, double expected, double tolerance = 0.0000005)
{
    if (std::abs(actual - expected) > tolerance)
    {
        std::cerr << "expected " << expected << ", actual " << actual << '\n';
        std::abort();
    }
}

LayeredScene Fresh()
{
    const Config config;
    LayeredScene scene;
    assert(config.minimumTransitionSeconds == 13.0f);
    assert(scene.minimumTransitionSeconds == config.minimumTransitionSeconds);
    assert(scene.elapsed == 0.0 && scene.displayedProgress == 0.0);
    assert((config.transitionHeights == std::array<float, 4>{5, 15, 25, 35}));
    scene.heights = config.transitionHeights;
    scene.minimumTransitionSeconds = config.minimumTransitionSeconds;
    return scene;
}

void StepToward(LayeredScene& scene, float dt, float height)
{
    const double before = scene.displayedProgress;
    const double target = scene.ProgressForHeight(height);
    scene.Update(dt, height);
    assert(scene.displayedProgress >= std::min(before, target) - 1e-12);
    assert(scene.displayedProgress <= std::max(before, target) + 1e-12);
    assert(std::abs(scene.displayedProgress - before) <= double(dt) / 13.0 + 1e-12);
}

void FrameRateBoundaries(int fps)
{
    const float dt = 1.0f / fps;
    LayeredScene adjacent = Fresh();
    for (int frame = 0; frame < 13 * fps - 1; ++frame)
        StepToward(adjacent, dt, 20);
    assert(adjacent.displayedProgress < 1.0);
    adjacent.Update(dt, 20);
    Near(adjacent.displayedProgress, 1.0);
    Near(LayeredScene::Fade(adjacent.displayedProgress), 1.0);
    assert(LayeredScene::Fade(adjacent.displayedProgress - 1) == 0);

    LayeredScene skip = Fresh();
    for (int frame = 0; frame < 13 * fps; ++frame)
        StepToward(skip, dt, 40);
    Near(skip.displayedProgress, 1.0);
    Near(LayeredScene::Fade(skip.displayedProgress), 1.0);
    Near(LayeredScene::Fade(skip.displayedProgress - 1), 0.0);
    for (int frame = 0; frame < 13 * fps - 1; ++frame)
        StepToward(skip, dt, 40);
    assert(skip.displayedProgress < 2.0);
    skip.Update(dt, 40);
    Near(skip.displayedProgress, 2.0);
    Near(LayeredScene::Fade(skip.displayedProgress - 1), 1.0);

    for (int frame = 0; frame < 13 * fps; ++frame)
        StepToward(skip, dt, 0);
    Near(skip.displayedProgress, 1.0);
    for (int frame = 0; frame < 13 * fps - 1; ++frame)
        StepToward(skip, dt, 0);
    assert(skip.displayedProgress > 0.0);
    skip.Update(dt, 0);
    Near(skip.displayedProgress, 0.0);

    std::cout << "PASS: " << fps << " FPS, adjacent 13 s, two backgrounds 26 s, reverse 26 s\n";
}

int main()
{
    for (int fps : {30, 60, 144}) FrameRateBoundaries(fps);

    LayeredScene scene = Fresh();
    Near(scene.ProgressForHeight(-100), 0);
    Near(scene.ProgressForHeight(10), .5);
    Near(scene.ProgressForHeight(20), 1);
    Near(scene.ProgressForHeight(30), 1.5);
    Near(scene.ProgressForHeight(100), 2);
    scene.Update(6.5f, 40);
    Near(scene.displayedProgress, .5);
    Near(LayeredScene::Fade(scene.displayedProgress), .5);
    for (int frame = 0; frame < 300; ++frame)
        StepToward(scene, 1.0f / 60, 0);
    Near(scene.displayedProgress, .5 - 5.0 / 13.0);
    for (int frame = 0; frame < 600; ++frame)
        StepToward(scene, 1.0f / 60, 10);
    Near(scene.displayedProgress, .5);
    for (int frame = 0; frame < 600; ++frame)
        StepToward(scene, 1.0f / 60, 7.5f);
    Near(scene.displayedProgress, .25);
    std::cout << "PASS: mid-transition retargeting is monotonic and cannot overshoot\n";

    const double before = scene.displayedProgress;
    const double elapsedBefore = scene.elapsed;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    for (float invalidDt : {0.0f, -1.0f, nan, infinity, -infinity})
    {
        scene.Update(invalidDt, 40);
        assert(scene.displayedProgress == before && scene.elapsed == elapsedBefore);
    }
    for (float invalidHeight : {nan, infinity, -infinity})
    {
        scene.Update(1, invalidHeight);
        assert(scene.displayedProgress == before);
        assert(std::isfinite(scene.elapsed));
    }
    StepToward(scene, 1, 40);
    Near(scene.displayedProgress, before + 1.0 / 13.0);
    std::cout << "PASS: invalid/zero time freezes state; nonfinite heights preserve transition and recover\n";

    scene = Fresh();
    scene.minimumTransitionSeconds = 0;
    for (float height : {0.0f, 10.0f, 40.0f, 30.0f, 7.5f, 0.0f})
    {
        scene.Update(1.0f / 60, height);
        Near(scene.displayedProgress, scene.ProgressForHeight(height));
    }
    scene.Update(1, nan);
    assert(scene.displayedProgress == 0);
    scene.Update(0, 40);
    assert(scene.displayedProgress == 0);
    std::cout << "PASS: zero-second mode follows height immediately without invalid input poisoning\n";

    scene = Fresh();
    for (int frame = 1; frame <= 80 * 60; ++frame)
    {
        const double target = double(frame) / (80 * 60) * 2;
        const float height = static_cast<float>(target <= 1 ? 5 + 10 * target : 25 + 10 * (target - 1));
        StepToward(scene, 1.0f / 60, height);
        Near(scene.displayedProgress, target);
    }
    assert(scene.displayedProgress == 2);
    scene = Fresh();
    assert(scene.displayedProgress == 0 && scene.elapsed == 0);
    std::cout << "PASS: slow height progression follows its target without extra lag; fresh state resets to day\n";
}
'''


def main():
    # ミカ：本番の状態宣言と三つのメソッドを抽出し、GPUを使わず切替時間を検証する
    source = SOURCE.read_text(encoding="utf-8-sig")
    header = HEADER.read_text(encoding="utf-8-sig")
    layer = block(source, "struct BackgroundRenderer::LayeredScene")
    state = "\n".join(declaration(layer, name) for name in (
        "heights", "elapsed", "displayedProgress", "minimumTransitionSeconds"
    ))
    config = "\n".join(declaration(header, name) for name in (
        "transitionHeights", "minimumTransitionSeconds"
    ))
    methods = "\n".join(block(layer, signature) for signature in (
        "double ProgressForHeight(", "void Update(float dt, float height)", "static float Fade("
    ))
    harness = (PRELUDE + "struct Config {\n" + config + "\n};\n"
               + "struct LayeredScene {\n" + state + "\n" + methods + "\n};\n" + TESTS)
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    vs = subprocess.check_output([
        str(vswhere), "-latest", "-products", "*", "-requires",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"
    ], text=True).strip()
    vcvars = Path(vs) / "VC/Auxiliary/Build/vcvars64.bat"
    with tempfile.TemporaryDirectory(prefix="background_transition_") as temp:
        directory = Path(temp)
        (directory / "transition.cpp").write_text(harness, encoding="utf-8")
        command = f'call "{vcvars}" >nul && cl /nologo /std:c++20 /EHsc /utf-8 /W4 transition.cpp /Fe:transition.exe'
        (directory / "build.cmd").write_text(command, encoding="utf-8")
        subprocess.run(["cmd", "/d", "/c", "build.cmd"], cwd=directory, check=True)
        subprocess.run([str(directory / "transition.exe")], cwd=directory, check=True)


if __name__ == "__main__":
    main()
