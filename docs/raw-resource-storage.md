# Raw Resource Storage

Last updated: 2026-05-10

## Goal

Keep a reusable private raw resource vault in OneDrive, while keeping this repo
focused on converted/project-ready assets.

Unreal projects, Fab downloads, archives, and other bulky source packages are
temporary inputs. After everything useful is extracted, cataloged, and converted,
the original source project/archive may be deleted from the local machine.

## OneDrive Root

The OneDrive directory is machine-specific. If Codex does not know the path on
the current machine, it must ask before copying or moving files.

Expected root inside OneDrive:

```text
RawResourceVault/
```

## Vault Layout

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

## Folder Meaning

- `inbox/`: temporary untouched downloads, Unreal projects, Fab packs, zips.
- `extracted/`: durable source resources pulled out of packages.
- `converted/`: converted files that are not yet committed into a project.
- `catalog/`: global indexes and license summaries.
- `quarantine/`: unclear license/source, broken imports, UE-only content, or
  anything we should not use yet.

## Package Manifest

Every extracted package must have `manifest.json`:

```json
{
  "id": "fab.package_slug",
  "name": "Package Name",
  "source": {
    "kind": "fab",
    "url": "",
    "author": "",
    "downloaded": "2026-05-10"
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
    "targetProject": "my_engine_sandbox",
    "convertedAssets": []
  }
}
```

## Unreal/Fab Rule

For Unreal projects from standard Fab/Marketplace licenses:

1. Extract and catalog everything useful first.
2. Save source URL, package name, license, author/vendor, and whether it is
   UE-only.
3. Convert reusable assets into this engine/project format.
4. Keep extracted source files and manifests in OneDrive.
5. Delete the original Unreal project only after extraction is complete.

The user says only standard-license content is being placed there, but Codex
should still ask if a package has no source URL/license note.

## Repo Rule

This repo should keep:

- converted assets under `assets/`;
- project docs under `docs/`;
- lightweight raw samples only when useful and allowed;
- no bulky Unreal projects after extraction.

Current raw inventory:

- curated summary: `docs/raw-assets-inventory.md`;
- generated full scan: `docs/raw-assets-inventory.generated.md`.
