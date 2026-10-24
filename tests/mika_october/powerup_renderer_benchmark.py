from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess


ROOT = Path(__file__).resolve().parents[2]
SNAPSHOT = ROOT / "build/mika_october/optimization-before/HEWGame"
OUTPUT = ROOT / "build/mika_october/powerup-benchmark"


def read(path):
    return path.read_text(encoding="utf-8-sig")


def compile_variant(variant, vcvars):
    directory = OUTPUT / variant
    directory.mkdir(parents=True, exist_ok=True)
    source_root = SNAPSHOT if variant == "before" else ROOT / "HEWGame"
    renderer = read(source_root / "Game/PowerUp/PowerUpRenderer.cpp")
    renderer = "#include <cstdint>\nextern std::uint64_t g_drawCalls;\n" + renderer
    draw = "pContext->Draw(vertexBuffer.GetCount(), 0);"
    assert renderer.count(draw) == 1
    renderer = renderer.replace(draw, "++g_drawCalls; " + draw)
    shader = read(source_root / "Engine/Render/Shader.cpp")
    shader = "#include <cstdint>\nextern std::uint64_t g_uploadCalls;\n" + shader
    update = "GetContext()->UpdateSubresource(m_pBuffers[slot], 0, nullptr, pData, 0, 0);"
    assert shader.count(update) == 1
    shader = shader.replace(update, "++g_uploadCalls, " + update)
    (directory / "PowerUpRenderer.cpp").write_text(renderer, encoding="utf-8")
    (directory / "PowerUpRenderer.h").write_text(read(source_root / "Game/PowerUp/PowerUpRenderer.h"), encoding="utf-8")
    (directory / "Shader.cpp").write_text(shader, encoding="utf-8")
    (directory / "ShaderList.h").write_text("#pragma once\n#include <DirectXMath.h>\nclass ShaderList { public: static void SetWVP(DirectX::XMFLOAT4X4*); };\n", encoding="utf-8")
    sources = [
        ROOT / "tests/mika_october/powerup_renderer_benchmark.cpp",
        directory / "PowerUpRenderer.cpp", directory / "Shader.cpp",
    ]
    sources += [ROOT / "HEWGame" / item for item in (
        "Engine/Render/Texture.cpp", "Engine/Render/TextureDX.cpp", "Engine/Render/TextureCache.cpp",
        "Engine/Render/MeshDX.cpp", "Engine/Render/DrawMatrix.cpp",
        "Engine/Core/VertexBuffer.cpp", "Engine/Core/IndexBuffer.cpp",
    )]
    includes = [directory] + [ROOT / "HEWGame" / item for item in (
        "Engine", "Engine/Core", "Engine/Render", "Engine/Camera", "DirectXTex",
    )]
    command = f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
    command += "cl /nologo /std:c++20 /EHsc /MT /O2 /utf-8 /W4 /DNOMINMAX /DUNICODE /D_UNICODE "
    command += f"/DPOWERUP_BENCH_BEFORE={int(variant == 'before')} "
    command += " ".join(f'/I"{path}"' for path in includes) + " "
    command += ' '.join(f'"{path}"' for path in sources)
    command += f' /Fe:powerup_benchmark.exe "{ROOT / "HEWGame/DirectXTex/x64/Release/DirectXTex.lib"}" ole32.lib windowscodecs.lib gdi32.lib user32.lib /link /LIBPATH:"{ROOT / "HEWGame"}"\n'
    (directory / "build.cmd").write_text(command, encoding="utf-8")
    with (directory / "compile.log").open("w", encoding="utf-8") as log:
        result = subprocess.run(["cmd", "/d", "/c", "build.cmd"], cwd=directory, stdout=log, stderr=subprocess.STDOUT)
    if result.returncode:
        print("\n".join(line for line in read(directory / "compile.log").splitlines() if "error" in line.lower()))
        raise RuntimeError(f"{variant} benchmark compilation failed")
    print(f"Built {variant}: {directory / 'powerup_benchmark.exe'}", flush=True)


def run_variant(variant, trial):
    directory = OUTPUT / variant
    result_dir = directory / f"trial-{trial}"
    result_dir.mkdir(exist_ok=True)
    with (result_dir / "run.log").open("w", encoding="utf-8") as log:
        subprocess.run([str(directory / "powerup_benchmark.exe"), str(result_dir)],
                       cwd=ROOT / "HEWGame", stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
    print(read(result_dir / "run.log").strip(), flush=True)
    return json.loads(read(result_dir / "result.json"))


def summarize(results):
    before = (OUTPUT / "before/trial-1/frame.rgba").read_bytes()
    after = (OUTPUT / "after/trial-1/frame.rgba").read_bytes()
    assert len(before) == len(after) == 960 * 540 * 4
    differences = [abs(a - b) for a, b in zip(before, after)]
    changed = sum(any(differences[i:i + 4]) for i in range(0, len(differences), 4))
    report = {
        "scope": "isolated PowerUp UI only; real renderer, mesh, texture and shader bodies; no video, effects, physics, save or Present",
        "method": "960x540, 6 fixed cards, currency 12345, 120 warm frames + 600 samples per trial; GPU drained outside each CPU submission sample",
        "baseline": str(SNAPSHOT),
        "results": results,
        "pixels": {"changed_pixels": changed, "max_channel_difference": max(differences),
                   "before_sha256": hashlib.sha256(before).hexdigest(), "after_sha256": hashlib.sha256(after).hexdigest()},
    }
    (OUTPUT / "summary.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-only", action="store_true")
    parser.add_argument("--run-only", action="store_true")
    parser.add_argument("--trials", type=int, default=3)
    args = parser.parse_args()
    if not args.run_only:
        vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
        vs = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
        vcvars = Path(vs) / "VC/Auxiliary/Build/vcvars64.bat"
        compile_variant("before", vcvars)
        compile_variant("after", vcvars)
    if args.build_only:
        return
    results = {"before": [], "after": []}
    for trial in range(1, args.trials + 1):
        # ミカ：実行順を交互にし、起動順や発熱だけによる偏りを抑える
        for variant in (("before", "after") if trial % 2 else ("after", "before")):
            results[variant].append(run_variant(variant, trial))
    summarize(results)


if __name__ == "__main__":
    main()
