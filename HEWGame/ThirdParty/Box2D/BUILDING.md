# Box2D ビルド & 配置ルール

## 配置ルール（source of truth）

```
HEWGame/ThirdParty/Box2D/
  include/   <- HEWGame/box2d/include をそのままコピー
  lib/
    Win32/
      Debug/   <- box2dd.lib
      Release/ <- box2d.lib
    x64/
      Debug/   <- box2dd.lib
      Release/ <- box2d.lib
```

- `ThirdParty/Box2D/include` は **必ず** `HEWGame/box2d/include` のコピーとする。
- Debug では `box2dd.lib`、Release では `box2d.lib` が生成される前提。
- 生成物は上記の `lib/<platform>/<config>/` に配置する。

## ビルド手順（Visual Studio / CMake）

### A. create_sln.bat を使う（既定は Win32）

```
cd HEWGame/box2d
create_sln.bat
```

- `HEWGame/box2d/build/box2d.sln` が生成されるので、VS2022 で開く。
- 構成 `Debug/Release`、プラットフォーム `Win32` をビルドする。

> x64 用は **別ビルドディレクトリ** を作成することを推奨。

### B. CMake で Win32/x64 を分けて生成する（推奨）

```
cmake -S HEWGame/box2d -B HEWGame/box2d/build_win32 -A Win32
cmake -S HEWGame/box2d -B HEWGame/box2d/build_x64 -A x64

cmake --build HEWGame/box2d/build_win32 --config Debug
cmake --build HEWGame/box2d/build_win32 --config Release
cmake --build HEWGame/box2d/build_x64 --config Debug
cmake --build HEWGame/box2d/build_x64 --config Release
```

- 生成物は各 `build_*/src/<Config>/` 配下に出力される。

## ThirdParty へのコピー

### 手動コピー

- `HEWGame/box2d/include` → `HEWGame/ThirdParty/Box2D/include`
- `HEWGame/box2d/<build_root>/src/Debug/box2dd.lib` → `HEWGame/ThirdParty/Box2D/lib/<platform>/Debug/`
- `HEWGame/box2d/<build_root>/src/Release/box2d.lib` → `HEWGame/ThirdParty/Box2D/lib/<platform>/Release/`

### コピー用スクリプト（Windows）

`HEWGame/ThirdParty/Box2D/copy_box2d_artifacts.bat` を使うことで再現可能に配置できます。

```
rem Usage: copy_box2d_artifacts.bat [build_root] [platform] [config]
rem 例: Win32 Debug
HEWGame/ThirdParty/Box2D/copy_box2d_artifacts.bat HEWGame/box2d/build_win32 Win32 Debug

rem 例: x64 Release
HEWGame/ThirdParty/Box2D/copy_box2d_artifacts.bat HEWGame/box2d/build_x64 x64 Release
```

- `config=Debug` の場合は `box2dd.lib`、`config=Release` の場合は `box2d.lib` をコピーします。
