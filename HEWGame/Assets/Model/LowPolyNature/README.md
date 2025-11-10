# LowPolyNature Asset Guidelines

This folder hosts the LowPolyNature asset pack. Keep the following structure when adding or updating files:

- `Models/`
  - Contains all 3D mesh and rig assets (`.fbx`, `.pmx`, etc.).
  - Place animation variants alongside their base meshes.
- `Textures/`
  - Contains all texture resources (`.png`, `.jpg`, etc.).
  - Keep authored textures and baked maps together; prefix related files consistently so they sort naturally.
- `References/`
  - Store documentation, license information, and external resource links (`.txt`, `.pdf`, etc.).

When importing new assets:
1. Match the appropriate subfolder based on file type before committing.
2. Update any hard-coded paths in engine code or data files to the new location under the relevant subfolder.
3. Keep file names stable to avoid breaking serialized references.

Following these rules keeps build scripts and content pipelines aligned with the expected layout.
