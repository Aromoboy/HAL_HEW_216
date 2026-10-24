"""ミカ：実際のHLSLをメモリ内で比較し、命令数をFPSの改善率と混同しない"""
import ctypes as ct
import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
compiler = ct.WinDLL("d3dcompiler_47.dll")
compiler.D3DCompile.argtypes = [ct.c_void_p, ct.c_size_t, ct.c_char_p, ct.c_void_p,
    ct.c_void_p, ct.c_char_p, ct.c_char_p, ct.c_uint, ct.c_uint,
    ct.POINTER(ct.c_void_p), ct.POINTER(ct.c_void_p)]
compiler.D3DCompile.restype = ct.c_long
compiler.D3DDisassemble.argtypes = [ct.c_void_p, ct.c_size_t, ct.c_uint,
    ct.c_char_p, ct.POINTER(ct.c_void_p)]
compiler.D3DDisassemble.restype = ct.c_long


def method(blob, slot, result):
    table = ct.cast(blob, ct.POINTER(ct.POINTER(ct.c_void_p))).contents
    return ct.WINFUNCTYPE(result, ct.c_void_p)(table[slot])


def data(blob):
    return method(blob, 3, ct.c_void_p)(blob), method(blob, 4, ct.c_size_t)(blob)


def release(blob):
    if blob:
        method(blob, 2, ct.c_ulong)(blob)


def instructions(source, target, flags):
    blob, error, assembly = ct.c_void_p(), ct.c_void_p(), ct.c_void_p()
    try:
        result = compiler.D3DCompile(source, len(source), None, None, None,
            b"main", target.encode(), flags, 0, ct.byref(blob), ct.byref(error))
        assert result >= 0, ct.string_at(*data(error)).decode(errors="replace") if error else result
        pointer, size = data(blob)
        result = compiler.D3DDisassemble(pointer, size, 0, None, ct.byref(assembly))
        assert result >= 0, result
        output = ct.string_at(*data(assembly)).decode(errors="replace")
        match = re.search(r"Approximately (\d+) instruction slots used", output)
        assert match, output
        return int(match.group(1))
    finally:
        release(assembly)
        release(error)
        release(blob)


def main():
    compile_source = (ROOT / "HEWGame/Engine/Render/Shader.cpp").read_text(encoding="utf-8-sig")
    assert re.search(r"#if[^\n]*_DEBUG[\s\S]*?D3DCOMPILE_DEBUG[\s\S]*?#else[\s\S]*?D3DCOMPILE_OPTIMIZATION_LEVEL3[\s\S]*?#endif", compile_source)
    shaders = (ROOT / "HEWGame/Engine/Render/ShaderList.cpp").read_text(encoding="utf-8-sig")
    for name, target in (("MakeWorldVS", "vs_5_0"), ("MakeAnimeVS", "vs_5_0"),
            ("MakeLambertPS", "ps_5_0"), ("MakeLambertLegacyPS", "ps_5_0")):
        section = shaders[shaders.index("void ShaderList::" + name + "()") :]
        source = section.split('R"EOT(', 1)[1].split(')EOT"', 1)[0].encode()
        original, optimized = instructions(source, target, 5), instructions(source, target, 32768)
        assert optimized <= original, (name, original, optimized)
        print(json.dumps({"shader": name, "old_instruction_slots": original,
            "release_instruction_slots": optimized}))
    print("PASS Release optimized, Debug retained; instruction counts are not FPS measurements")


if __name__ == "__main__":
    main()
