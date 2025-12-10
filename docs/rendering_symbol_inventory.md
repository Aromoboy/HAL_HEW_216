# Rendering/Model symbol occurrence inventory

The following inventory captures all `rg` matches for migration planning around model, texture, and mesh handling.

## `#include "Model.h"`
- `HEWGame/include/TestObject.h` (line 4) — TestObject keeps a `std::unique_ptr<Model>` for scene objects.
- `HEWGame/include/ShaderList.h` (line 5) — ShaderList uses `Model::Material` helpers.
- `HEWGame/include/SceneGame.h` (line 6) — Scene bootstrapper pulls in Model for stage objects.
- `HEWGame/src/Model.cpp` (line 1) — Model implementation file.

## `TextureLoad.h`
- `HEWGame/src/Texture.cpp` (line 2) — Texture loading helpers included by Texture class.
- `HEWGame/src/Model.cpp` (line 2) — Model loader uses texture loading utilities.

## `Texture.h`
- `HEWGame/include/SpriteRenderer.h` (line 10) — Sprite renderer interface relies on Texture.
- `HEWGame/include/Sprite.h` (line 8) — Sprite utility uses Texture and MeshBuffer bindings.
- `HEWGame/include/Shader.h` (line 5) — Shader interface exposes texture binding.
- `HEWGame/TimerDisplay.h` (line 3) — HUD timer uses a texture atlas for digits.
- `HEWGame/src/DirectX.cpp` (line 2) — DirectX helpers include Texture utilities.
- `HEWGame/src/Core/DXContext.cpp` (line 7) — DXContext references Texture defaults.
- `HEWGame/src/BackgroundRenderer.cpp` (line 5) — Background renderer consumes Texture for sprite drawing.
- `HEWGame/src/HudRenderer.cpp` (line 5) — HUD renderer manages textures for UI elements.
- Project metadata: `HEWGame/DX22_Project.vcxproj` (line 341) and `.filters` (line 542) list `include/Texture.h` as a header item.

## `MeshBuffer`
- Definition and lifecycle: `HEWGame/include/MeshBuffer.h` (lines 8–41) declare the mesh buffer class and description; implementation in `HEWGame/src/MeshBuffer.cpp` (lines 7–124).
- Model integration: `HEWGame/include/Model.h` (lines 17–34, 111–114) embeds `MeshBuffer*` inside `Model::Mesh` records; `HEWGame/src/Model.cpp` creates buffers during load (lines 321–333).
- Geometry helpers: `HEWGame/include/Geometory.h` (lines 6, 48–60) hold static `MeshBuffer*` fields; `HEWGame/src/Geometory.cpp` allocates and draws buffers (lines 7–456).
- Collision support: `HEWGame/include/Collision/ConvexHullBuilder.h` (lines 6, 24–33) accepts `MeshBuffer::Description`; `HEWGame/src/Collision/ConvexHullBuilder.cpp` processes descriptions and index reads (e.g., lines 255–456).
- Box2D conversion: `HEWGame/include/Systems/Box2DMeshBuilder.h` (lines 10, 38–40) and `HEWGame/src/Systems/Box2DMeshBuilder.cpp` (lines 38–180) transform mesh data into physics polygons.
- Sprites: `HEWGame/include/Sprite.h` (lines 6, 31–40) embeds shared `MeshBuffer` for sprite geometry.
- Game systems: `HEWGame/src/SweetsManager.cpp` includes MeshBuffer (lines 35, 1004–1006) to build colliders.
- Project metadata: `HEWGame/DX22_Project.vcxproj` (lines 208, 314) and `.filters` (lines 156, 425) list `MeshBuffer` sources/headers.

## `Model::Material`
- Type definition: `HEWGame/include/Model.h` (lines 24–31) defines the material payload stored in models.
- Accessors: `HEWGame/src/Model.cpp` exposes material getters (lines 522–537).
- Shader bridging: `HEWGame/include/ShaderList.h` (lines 42–45) and `HEWGame/src/ShaderList.cpp` (lines 32–112) set shader parameters from `Model::Material`.

## `SetMaterial(const Model::Material&)`
- Declaration: `HEWGame/include/ShaderList.h` (line 44) exposes the setter.
- Definition: `HEWGame/src/ShaderList.cpp` (lines 84–97) writes diffuse/ambient/specular values and binds the texture.

## `Model::Draw`
- Implementation: `HEWGame/src/Model.cpp` (lines 452–493) binds shaders, auto-selects textures, and draws meshes or subsets.

## `TextureLoad::Load`
- No occurrences found via `rg "TextureLoad::Load"`.

## `Texture::Create`
- File-based loader: `HEWGame/src/Texture.cpp` (lines 20–48) loads from disk and creates SRV.
- Memory-based loader: `HEWGame/src/Texture.cpp` (lines 50–80) creates from in-memory data.
- Typed texture creation: `HEWGame/src/Texture.cpp` (lines 81–141) builds textures from explicit formats and dimensions and wires SRV/RTV/DSV helpers.
- HUD usage: `HEWGame/src/HudRenderer.cpp` calls `Texture::Create` for dynamic UI texture allocation (line 52).
