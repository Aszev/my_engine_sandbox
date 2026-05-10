---
name: raw-resource-curator
description: Catalog, extract, normalize, and preserve source/license metadata for raw game resources, asset packs, Fab/Unreal projects, archives, GLB/FBX/OBJ models, textures, audio, animations, and converted engine assets. Use when Codex needs to inventory raw resources, extract everything useful from temporary source projects before deletion, build source manifests, organize OneDrive raw asset storage, or prepare assets for conversion into project formats.
---

# Raw Resource Curator

## Core Rule

Treat raw source packs as temporary inputs and converted/project-ready resources
as durable outputs. Never rely on memory or folder names for provenance: create
or update a manifest before moving on.

If the user mentions OneDrive and the path is unknown, ask for the machine-local
OneDrive resource root before copying files there.

## Storage Model

Use this durable structure under the user's OneDrive resource root:

```text
RawResourceVault/
  catalog/
    packages.csv
    assets.csv
    licenses/
  inbox/
    source-name/package-name/
  extracted/
    category/package-slug/
      manifest.json
      README.source.md
      source/
      textures/
      models/
      animations/
      audio/
      materials/
      maps/
      docs/
  converted/
    project-slug/category/asset-slug/
      manifest.json
      files...
  quarantine/
```

Meanings:

- `inbox/`: temporary downloads, Unreal projects, archives, untouched vendor
  packages. Can be deleted after extraction if `extracted/.../manifest.json`
  confirms all useful assets were captured.
- `extracted/`: durable raw resources, normalized by category and package.
- `converted/`: outputs converted to this engine/project format.
- `catalog/`: cross-package index and license summaries.
- `quarantine/`: unknown license, broken archive, unclear provenance, or assets
  that should not be used yet.

## Required Manifest

Every package gets `manifest.json` and `README.source.md`.

Manifest fields:

```json
{
  "id": "source.package_slug",
  "name": "Human Package Name",
  "source": {
    "kind": "fab|unreal-marketplace|opengameart|polyhaven|local|unknown",
    "url": "",
    "author": "",
    "downloaded": "YYYY-MM-DD"
  },
  "license": {
    "name": "Fab Standard License",
    "ueOnly": false,
    "redistribution": "embedded-only",
    "attributionRequired": false,
    "notes": ""
  },
  "temporarySourceRemoved": false,
  "contents": {
    "models": [],
    "textures": [],
    "animations": [],
    "audio": [],
    "materials": [],
    "maps": [],
    "docs": []
  },
  "conversion": {
    "targetProject": "",
    "convertedAssets": []
  }
}
```

If source/license is unclear, ask the user where it came from and place the
package in `quarantine/` until clarified.

## Unreal/Fab Extraction Workflow

For Unreal projects or Fab packs:

1. Confirm or infer license. If unclear, ask for source URL/license.
2. Check for UE-only restrictions. If `UE-Only` is true, do not convert to this
   engine unless the user later provides a compatible license.
3. Inventory the project before deleting anything:
   - `.uasset`, `.umap`, `.uexp`, `.ubulk`;
   - source `.fbx`, `.glb`, `.gltf`, `.obj`, `.abc`;
   - textures `.png`, `.jpg`, `.tga`, `.exr`, `.hdr`, `.dds`;
   - audio `.wav`, `.ogg`, `.mp3`;
   - docs/license/readme files.
4. Extract/export everything reusable:
   - meshes, skeletons, animations;
   - materials and texture dependencies;
   - maps only as reference unless there is an importer;
   - preview thumbnails/screenshots if useful for catalog browsing.
5. Save extracted files into `extracted/<category>/<package-slug>/`.
6. Write manifest and README with source URL, license, author, and notes.
7. Convert into `assets/` or `converted/` only after extraction is complete.
8. Mark `temporarySourceRemoved: true` only after the user deletes the Unreal
   project or explicitly approves deletion.

Spend the time to extract broadly once; assume the original Unreal project may
be deleted after this pass.

## Repo Workflow

When working inside a project repo:

- Keep imported engine-ready assets under the project `assets/`.
- Keep local raw source scans documented under `docs/` when OneDrive root is not
  available yet.
- Do not move large raw resources into engine plugin samples unless their
  license allows redistributing raw assets.
- Prefer small CC0/default-safe assets for engine samples.

## Scripts

Use `scripts/inventory_raw.py` to scan a raw folder and emit a markdown
inventory. It uses only Python stdlib and can inspect zip archives plus PNG/JPEG
dimensions.

Example:

```bash
python scripts/inventory_raw.py --root raw --output docs/raw-assets-inventory.md
```
