from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]
mode = sys.argv[1] if len(sys.argv) > 1 else "after"
if mode not in ("before", "after"):
    raise SystemExit("usage: gameplay_optimization_regression.py [before|after]")
source_root = ROOT / "build/mika_october/optimization-before" if mode == "before" else ROOT
output = ROOT / "build/mika_october" / f"gameplay_optimization_{mode}"
output.mkdir(parents=True, exist_ok=True)

# ミカ：既存の実物理ビルド手順を共用し、比較時だけ保存済みの本番ソースを選ぶ
command = (ROOT / "tests/mika_october/run_cream_physics_regression.cmd").read_text(encoding="utf-8-sig")
command = command.replace('pushd "%~dp0..\\.."', f'pushd "{ROOT}"')
command = command.replace("cream_physics_regression", "gameplay_optimization_regression")
command = command.replace("mika_october\\cream_physics", f"mika_october\\gameplay_optimization_{mode}")
if mode == "after":
    command = command.replace("/D_WINDOWS", "/D_WINDOWS /DMIKA_OPTIMIZED")
for directory in ("Engine", "Game"):
    command = command.replace(f"HEWGame\\{directory}", str(source_root / "HEWGame" / directory))
if "--test-only" in sys.argv:
    start = command.index("cl /nologo")
    end = command.index(" >build\\", start)
    command = command[:start] + "\n".join(command[start:end].splitlines()[:3]).rstrip(" ^") + command[end:]
batch = output / "build.cmd"
batch.write_text(command, encoding="utf-8")
result = subprocess.run(["cmd.exe", "/d", "/c", str(batch)], cwd=ROOT,
                        capture_output=True, text=True, encoding="utf-8", errors="replace")
(output / "run.log").write_text(result.stdout + result.stderr, encoding="utf-8")
print(result.stdout, end="")
print(result.stderr, end="", file=sys.stderr)
if result.returncode:
    for filename in ("compile.log", "link.log"):
        path = output / filename
        if path.exists():
            print("\n".join(path.read_text(errors="replace").splitlines()[-20:]), file=sys.stderr)
raise SystemExit(result.returncode)
