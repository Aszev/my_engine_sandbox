# Project Asset Storage

## Current State

- Code, configs, scenes, prefabs, docs, skills, and lightweight descriptors live in the GitHub repo.
- Raw/source resources are curated outside the repo in OneDrive.
- `project.local.json` is ignored and already used for machine-local paths.
- Runtime virtual paths use aliases such as `asset://`, `project://`, and resource aliases from `project.json`.

## Target

Heavy project assets must live outside the Git repo:

```text
OneDrive/My_Projects/
  Sources/
    <prepared source packs and extracted source assets>
  Projects/
    <project-name>/
      assets/
      catalog/
      inbox/
```

For this machine:

```text
C:\OneDrives\OneDrive - Wargaming.net\My_Projects\Sources
C:\OneDrives\OneDrive - Wargaming.net\My_Projects\Projects\my_engine_sandbox
```

## Runtime Design

`project.json` stays portable and contains only project identity, plugins, startup world, and logical resource aliases.

`project.local.json` stores machine-local roots:

```json
{
  "version": 1,
  "engineRoot": "../my_engine/",
  "assetRoot": "C:/OneDrives/OneDrive - Wargaming.net/My_Projects/Projects/my_engine_sandbox/assets",
  "assetSourcesRoot": "C:/OneDrives/OneDrive - Wargaming.net/My_Projects/Sources",
  "assetProjectsRoot": "C:/OneDrives/OneDrive - Wargaming.net/My_Projects/Projects"
}
```

Runtime aliases:

- `asset://` points to `assetRoot` from `project.local.json`, or repo `assets/` if no local override exists.
- `asset_root://` points to the resolved runtime asset root.
- `project_assets://` points to the repo `assets/` folder for lightweight committed descriptors/placeholders.
- `asset_sources://` points to the prepared imported asset source root when configured.
- `asset_projects://` points to the OneDrive project asset parent when configured.

## Gaps

- Existing asset references still mostly use `asset://`; after the external copy is stable, heavy files should be removed from tracked repo paths.
- The editor/import tooling should create or update a project asset manifest when copying files into OneDrive.
- CI should run without OneDrive by falling back to repo `assets/` and test fixtures.

## Rules

- Do not commit heavy models, textures, audio, raw packs, Unreal projects, or archives into the project repo.
- Import from `Sources/` into `Projects/<project-name>/assets/`; do not reference temporary source folders directly from game content.
- Keep provenance in a manifest: source URL, license, author, original package, conversion date, and destination paths.
- If a project asset root is unknown on a machine, ask for it before copying heavy files.
- If license/source is unclear, put the asset in quarantine or leave it in `Sources/` until clarified.

## Migration Plan

1. Configure `project.local.json` on each machine.
2. Create `Projects/<project-name>/assets`.
3. Copy current working assets there as the initial project asset store.
4. Route new imports through the `project-resource-adder` skill.
5. Gradually replace repo-heavy assets with lightweight descriptors, samples, or ignored local copies.

## Sandbox Migration Status

Done on this machine:

- `assetRoot` is configured to
  `C:/OneDrives/OneDrive - Wargaming.net/My_Projects/Projects/my_engine_sandbox/assets`.
- Current repo `assets/` were mirrored into the external project asset store.
- Current `raw/water` sources were mirrored into
  `C:/OneDrives/OneDrive - Wargaming.net/My_Projects/Sources/raw/water`.
- Repo tracking was removed for `raw/`, `assets/fab/`, and
  `assets/characters/`; local copies remain on disk but are ignored.
- `git ls-files assets raw` has no tracked files larger than 100 KB after the
  migration.
