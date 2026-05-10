---
name: project-resource-adder
description: Add models, textures, audio, animations, prefabs, converted assets, and other heavy resources to a project that keeps code in GitHub but stores project asset copies in a machine-local OneDrive asset root. Use when importing or adding a resource to the engine project, when deciding where an asset should live, or when updating project asset manifests.
---

# Project Resource Adder

## Core Rule

Never commit heavy resources to the project repo. Keep Git for code,
configuration, scenes, prefabs, lightweight descriptors, docs, and scripts.
Copy heavy project-ready files into the external project asset store.

If the project asset root is unknown, ask the user for it before copying.

## Storage Model

Use this layout:

```text
<assetProjectsRoot>/<project-name>/
  assets/
    <category>/<asset-slug>/...
  catalog/
    project_assets.json
  inbox/
```

Prepared source packs live separately:

```text
<assetSourcesRoot>/
```

The project local config should provide:

```json
{
  "assetRoot": "C:/.../Projects/<project-name>/assets",
  "assetSourcesRoot": "C:/.../Sources",
  "assetProjectsRoot": "C:/.../Projects"
}
```

## Workflow

1. Read `project.local.json`.
2. Resolve `assetRoot`. If it is missing, ask the user where the project asset
   directory is on this machine.
3. Check source/license metadata. If unclear, ask for source URL and license.
4. Copy the resource into `assetRoot/<category>/<asset-slug>/`.
5. Update `<project-store>/catalog/project_assets.json`.
6. Reference imported files through `asset://...` from project descriptors.
7. Keep raw/vendor/Unreal source projects out of the repo.

## Script

Use `scripts/add_project_resource.py` for deterministic copying and manifest
updates.

Example:

```bash
python .codex/skills/project-resource-adder/scripts/add_project_resource.py \
  --project-root . \
  --source "C:/OneDrives/OneDrive - Wargaming.net/My_Projects/Sources/soldier/model.glb" \
  --dest "characters/soldier/model.glb" \
  --source-url "https://example.invalid/pack" \
  --license "Fab Standard License"
```

The script refuses to write into the repo `assets/` folder unless explicitly
given that path as `assetRoot`; prefer the OneDrive project asset root.
