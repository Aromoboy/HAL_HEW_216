from pathlib import Path
import os
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
LOADER = "HEWGame/Engine/Render/AssimpModelLorder.cpp"


def load_method(source):
    start = source.index("std::shared_ptr<ModelDX> AssimpModelLorder::Load(")
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
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
std::atomic<int> liveImporters{}, importCalls{};
int conversions{};
bool failImport{};
struct aiMesh { bool HasTextureCoords(int) const { return true; } };
struct aiScene { unsigned int mNumMeshes{}, mNumMaterials{}; aiMesh** mMeshes{}; };
namespace Assimp {
class Importer {
    aiScene scene;
public:
    Importer() { ++liveImporters; }
    ~Importer() { --liveImporters; }
    const aiScene* ReadFile(const char*, unsigned int) {
        ++importCalls;
        if (failImport) return nullptr;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return &scene;
    }
    const char* GetErrorString() const { return "controlled import failure"; }
};
}
struct ImportResult {
    std::unique_ptr<Assimp::Importer> importer;
    const aiScene* scene = nullptr;
    std::string error;
};
constexpr unsigned int aiProcess_Triangulate=1, aiProcess_FlipUVs=2,
    aiProcess_GenSmoothNormals=4, aiProcess_GenUVCoords=8;
struct ID3D11Device {};
struct TextureCache {};
struct MaterialTuningSettings {};
struct AABB {};
struct NodeDX {};
struct Animation { using AnimationArray = std::vector<int>; };
struct MeshDX {
    unsigned int GetMaterialID() const { return 0; }
    void SetMaterialID(unsigned int) {}
};
struct MaterialDX {
    MaterialDX(int, int, int, std::nullptr_t) {}
    void* GetTexture() const { return nullptr; }
    int GetDiffuse() const { return 0; }
    int GetAmbient() const { return 0; }
    int GetSpecular() const { return 0; }
};
struct ModelDX {
    ModelDX(const std::vector<std::shared_ptr<MeshDX>>&,
        const std::vector<std::shared_ptr<MaterialDX>>&,
        const MaterialTuningSettings*, std::optional<AABB>) {
        assert(liveImporters == 1);
        assert(conversions == 3);
    }
};
std::string NormalizePath(const std::string& value) { return value; }
std::optional<int> ResolveTextureOverrideForModel(const std::string&) { return {}; }
void ApplyMaterialTuning(MaterialDX&, const char*, const MaterialTuningSettings&) {}
namespace DebugLog {
std::string output;
void DebugMessage(const std::string& message) { output += message; }
}
class AssimpModelLorder {
public:
    std::shared_ptr<ModelDX> Load(const std::string&, ID3D11Device*, TextureCache&,
        const MaterialTuningSettings&);
    void MakeNode(const aiScene* scene, std::vector<NodeDX>&, Animation::AnimationArray&) {
        assert(scene && liveImporters == 1); ++conversions;
    }
    std::vector<std::shared_ptr<MeshDX>> MakeMesh(const aiScene* scene, AABB&, bool&,
        std::vector<NodeDX>&) {
        assert(scene && liveImporters == 1); ++conversions; return {};
    }
    std::vector<std::shared_ptr<MaterialDX>> MakeMaterial(const aiScene* scene,
        const std::string&, const std::string&, ID3D11Device*, TextureCache&,
        const MaterialTuningSettings&) {
        assert(scene && liveImporters == 1); ++conversions; return {};
    }
};
'''

TESTS = r'''
int main() {
    AssimpModelLorder loader;
    ID3D11Device device;
    TextureCache textures;
    MaterialTuningSettings settings;
    assert(!loader.Load("missing.fbx", &device, textures, settings));
    std::ofstream("empty.fbx");
    assert(!loader.Load("empty.fbx", &device, textures, settings));
    assert(importCalls == 0 && liveImporters == 0);
    { std::ofstream file("slow.fbx"); file << 'x'; }
    const auto begin = std::chrono::steady_clock::now();
    auto model = loader.Load("slow.fbx", &device, textures, settings);
    const auto elapsed = std::chrono::steady_clock::now() - begin;
    assert(model);
    assert(elapsed >= std::chrono::milliseconds(480));
    assert(importCalls == 1 && liveImporters == 0 && conversions == 3);
    failImport = true;
    conversions = 0;
    assert(!loader.Load("slow.fbx", &device, textures, settings));
    assert(importCalls == 2 && liveImporters == 0 && conversions == 0);
    assert(DebugLog::output.find("controlled import failure") != std::string::npos);
    std::cout << "PASS: 500ms import retained, importer lives through conversion, missing/empty/failed import cleanup\n";
}
'''


def main():
    # ミカ：実装本体を抽出し、遅いインポーターと変換時の寿命を制御して検証する
    source = (ROOT / LOADER).read_text(encoding="utf-8-sig")
    if "--baseline" in sys.argv:
        source = subprocess.check_output(
            ["git", "show", f"HEAD:{LOADER}"], cwd=ROOT
        ).decode("utf-8-sig")
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    vs = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
    vcvars = Path(vs) / "VC/Auxiliary/Build/vcvars64.bat"
    with tempfile.TemporaryDirectory(prefix="import_lifetime_") as temp:
        directory = Path(temp)
        (directory / "import.cpp").write_text(HARNESS + load_method(source) + TESTS, encoding="utf-8")
        command = f'call "{vcvars}" >nul && cl /nologo /std:c++20 /EHsc /utf-8 /W4 import.cpp /Fe:import.exe'
        (directory / "build.cmd").write_text(command, encoding="utf-8")
        subprocess.run(["cmd", "/d", "/c", "build.cmd"], cwd=directory, check=True)
        subprocess.run([str(directory / "import.exe")], cwd=directory, check=True)


if __name__ == "__main__":
    main()
