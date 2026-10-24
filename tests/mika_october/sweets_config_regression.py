from pathlib import Path
import json
import os
import subprocess


ROOT = Path(__file__).resolve().parents[2]
BEFORE = ROOT / "build/mika_october/mika-followup-before"
OUTPUT = ROOT / "build/mika_october/sweets_config"


def block(path, signature):
    source = path.read_text(encoding="utf-8-sig")
    start = source.index(signature)
    end = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def presets(path):
    entries = {}
    section = name = ""
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        line = line.split("#", 1)[0].strip()
        if line.startswith("["):
            section = line
        elif section == "[colliders.sweets.factory]" and "=" in line:
            key, value = (part.strip() for part in line.split("=", 1))
            if key == "preset":
                name = value
            elif key in ("scale", "padding", "rotationMinDegrees", "rotationMaxDegrees"):
                entries[name, key] = value
    return entries


def main():
    source = """
#include <cassert>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>
struct float3 { float x, y, z; };
"""
    for namespace, directory in (("Before", BEFORE), ("After", ROOT)):
        source += "namespace " + namespace + " {\n"
        for signature in ("bool TryParseFloat(", "std::string TrimToken(",
                          "bool ParseFloatList(", "bool ParseFloat3Value("):
            source += block(directory / "HEWGame/Game/Sweets/SweetsFactory.cpp", signature) + "\n"
        source += "}\n"
    source += """
int main() {
    for (const char* token : {"0.165.2", "1e3junk", "NaN", "inf", "1e9999", "", "1,2"}) {
        float value = 17;
        assert(!After::TryParseFloat(token, &value));
        assert(value == 17);
    }
    for (const char* token : {"0.165", " -2.5 ", "1.25e2", "0", "+3.5\\t"}) {
        float oldValue = 0, newValue = 0;
        assert(Before::TryParseFloat(token, &oldValue));
        assert(After::TryParseFloat(token, &newValue));
        assert(oldValue == newValue);
    }
    float oldMalformed = 0;
    assert(Before::TryParseFloat("0.165.2", &oldMalformed) && oldMalformed == 0.165f);
    float3 retained{7, 8, 9};
    assert(!After::ParseFloat3Value("0.22,0.165.2,0.165", retained));
    assert(retained.x == 7 && retained.y == 8 && retained.z == 9);
"""
    old = presets(BEFORE / "HEWGame/Assets/Config/gameplay.cfg")
    new = presets(ROOT / "HEWGame/Assets/Config/gameplay.cfg")
    assert old.keys() == new.keys()
    # ミカ：修正前に実際に使われた全プリセットの値を比較し、バナナの大きさも変えない
    for key, value in old.items():
        source += "{ float3 a{}, b{}; assert(Before::ParseFloat3Value(" + json.dumps(value) + ", a));\n"
        source += "assert(After::ParseFloat3Value(" + json.dumps(new[key]) + ", b));\n"
        source += "assert(a.x == b.x && a.y == b.y && a.z == b.z); }\n"
    source += f'std::puts("PASS: malformed numbers rejected, outputs preserved, {len(old)} preset vectors unchanged");\n}}\n'
    OUTPUT.mkdir(parents=True, exist_ok=True)
    (OUTPUT / "test.cpp").write_text(source, encoding="utf-8")
    vcvars = Path(os.environ["ProgramFiles"]) / "Microsoft Visual Studio/2022/Community/VC/Auxiliary/Build/vcvars64.bat"
    commands = f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\ncl /nologo /std:c++20 /EHsc /utf-8 /W4 test.cpp /Fe:test.exe >compile.log 2>&1\nif errorlevel 1 exit /b 1\ntest.exe >run.log 2>&1\nexit /b %errorlevel%\n'
    (OUTPUT / "run.cmd").write_bytes(commands.replace("\n", "\r\n").encode("utf-8"))
    subprocess.run(["cmd", "/d", "/c", "run.cmd"], cwd=OUTPUT, check=True)
    print((OUTPUT / "run.log").read_text(encoding="utf-8"))


if __name__ == "__main__":
    main()
