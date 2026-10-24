from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
mode = sys.argv[1] if len(sys.argv) > 1 else "after"
if mode not in ("before", "after"):
    raise SystemExit("usage: cream_removal_regression.py [before|after] [--benchmark] [--run-only]")
output = ROOT / "build/mika_october" / f"cream_removal_{mode}"
output.mkdir(parents=True, exist_ok=True)
executable = output / "cream_removal_regression.exe"
arguments = ["--benchmark"] if "--benchmark" in sys.argv else []

if "--run-only" not in sys.argv:
    # ミカ：共通の実物理ビルド手順を使い、比較前だけ接着実装の保存済み版を選ぶ
    command = (ROOT / "tests/mika_october/run_cream_physics_regression.cmd").read_text(encoding="utf-8-sig")
    command = command.replace('pushd "%~dp0..\\.."', f'pushd "{ROOT}"')
    command = command.replace("cream_physics_regression", "cream_removal_regression")
    command = command.replace("mika_october\\cream_physics", f"mika_october\\cream_removal_{mode}")
    if mode == "before":
        snapshot = ROOT / "build/mika_october/mika-followup-before/HEWGame/Game"
        command = command.replace('set "INCLUDE=', f'set "INCLUDE={snapshot};')
        command = command.replace("HEWGame\\Game\\Sweets\\SweetsContacts.cpp", str(snapshot / "Sweets/SweetsContacts.cpp"))
    if "--test-only" in sys.argv:
        start = command.index("cl /nologo")
        end = command.index(" >build\\", start)
        command = command[:start] + "\n".join(command[start:end].splitlines()[:3]).rstrip(" ^") + command[end:]
    batch = output / "build.cmd"
    batch.write_text(command, encoding="utf-8")
    result = subprocess.run(["cmd.exe", "/d", "/c", str(batch)], cwd=ROOT,
                            capture_output=True, text=True, encoding="utf-8", errors="replace")
    (output / "build-run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    print(result.stdout, end="")
    print(result.stderr, end="", file=sys.stderr)
    if result.returncode:
        for filename in ("compile.log", "link.log"):
            path = output / filename
            if path.exists():
                lines = path.read_text(errors="replace").splitlines()
                errors = [line for line in lines if "error " in line.lower()]
                print("\n".join(errors or lines[-10:]), file=sys.stderr)
        raise SystemExit(result.returncode)
    if not arguments:
        raise SystemExit(0)

result = subprocess.run([str(executable), *arguments], cwd=ROOT, capture_output=True,
                        text=True, encoding="utf-8", errors="replace")
(output / ("benchmark.log" if arguments else "run.log")).write_text(result.stdout + result.stderr, encoding="utf-8")
print(result.stdout, end="")
print(result.stderr, end="", file=sys.stderr)
raise SystemExit(result.returncode)
