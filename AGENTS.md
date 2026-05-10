# Project Agent Notes

This repository is an engine project. The engine is not vendored into the
project tree and project setup is driven by engine-owned scripts.

## Association

- Do not add project-local `Setup.ps1`, `Configure.ps1`, `Build.ps1`, or
  `Clean.ps1` wrappers.
- Associate this project with an engine checkout by running:

```powershell
<engine-root>\AssociateProject.ps1 -Project .\project.json
```

- The command writes `project.local.json`. Keep that file local and ignored.
- `project.local.json` must store `engineRoot` as a relative path from the
  project root.
- The engine and project may live in any directories and under any folder
  names. Do not assume sibling folders named `my_engine` or `sandbox_app`.
- For one-off commands, `ENGINE_ROOT` CMake cache value, `MY_ENGINE_ROOT`
  environment variable, or the app `--engine-root` argument may override the
  local association.
- Canonical engine documentation for this workflow lives at:
  `<engine-root>/docs/setup-and-project-association.md`.
- If the project fails to configure on a fresh machine, first inspect:
  `<engine-root>/README.md`,
  `<engine-root>/AGENTS.md`,
  `<engine-root>/docs/current-state.md`, and
  `<engine-root>/docs/setup-and-project-association.md`.

## Build

Generate project files from the engine root:

```powershell
<engine-root>\GenerateProjectFiles.ps1 -Project .\project.json
```

Build from the engine root:

```powershell
<engine-root>\BuildProject.ps1 -Project .\project.json
```

Clean the project build directory from the engine root:

```powershell
<engine-root>\CleanProject.ps1 -Project .\project.json
```

## Layout Rules

- Project-owned code, plugins, worlds, config, docs, lightweight asset
  descriptors, and build outputs stay in this project root.
- Heavy project assets must stay outside Git in the machine-local OneDrive
  project asset store configured by `project.local.json` `assetRoot`.
- `assetRoot` should point to
  `<assetProjectsRoot>/<project-name>/assets`; prepared source packs should
  live under `assetSourcesRoot`.
- Engine-owned code, engine assets, third-party source, and engine tools stay
  in the engine root.
- Engine resource aliases may use `engine:<relative-path-inside-engine>` when a
  project manifest needs to mount engine-owned resource folders.
- Runtime plugin requirements belong in `project.json`.
- Do not store absolute machine paths in tracked files.
- Do not commit heavy models, textures, audio, archives, Unreal projects, raw
  source packs, or converted asset blobs into the repo.

## New Machine Flow

1. Clone or copy the engine checkout.
2. Run `<engine-root>\Setup.ps1` once to bootstrap engine third-party
   dependencies.
3. Clone or copy this project checkout.
4. Run `<engine-root>\AssociateProject.ps1 -Project <project-root>\project.json`.
5. Add machine-local `assetRoot`, `assetSourcesRoot`, and `assetProjectsRoot`
   to ignored `project.local.json`.
6. Sync or create `<assetProjectsRoot>/<project-name>/assets`.
7. Run `<engine-root>\GenerateProjectFiles.ps1 -Project <project-root>\project.json`.
8. Run `<engine-root>\BuildProject.ps1 -Project <project-root>\project.json`.

## Agent Recovery Flow

When an agent opens this project on a new machine:

1. Read this file first.
2. Find the engine checkout. If `project.local.json` exists, use its
   `engineRoot`; otherwise ask for the engine root or inspect likely nearby
   folders without assuming fixed names.
3. Read the engine docs:
   - `<engine-root>/AGENTS.md`
   - `<engine-root>/README.md`
   - `<engine-root>/docs/current-state.md`
   - `<engine-root>/docs/setup-and-project-association.md`
4. If `<engine-root>/external_src` is missing, run `<engine-root>\Setup.ps1`.
5. Re-associate this project through
   `<engine-root>\AssociateProject.ps1 -Project <project-root>\project.json`.
6. Ensure `project.local.json` has `assetRoot`; ask the user for the OneDrive
   project asset path if it is missing.
7. Generate and build through the engine-owned scripts.

## Raw Folder

This project may reference `../raw` as an optional asset source for local
unprocessed files. A fresh checkout may not have that folder. Missing `raw/` is
not a setup problem; create it only when source assets or references need to be
imported. Runtime-critical assets must not depend on an implicit untracked
`raw/` folder.

## Project Skills

- Project-portable Codex skills live under `.codex/skills/`.
- `code-workflow-gate` is mandatory before any work that reads, changes,
  reviews, builds, tests, or reasons about source code.
- For engine/editor/runtime/plugin/ECS/input/camera/rendering/prefab/settings/
  build/asset-pipeline changes, `code-workflow-gate` must lead to
  `architecture-audit-first` before code edits.
- If a proposed code change would contradict or cast doubt on an accepted
  architecture document, coordinate/sign convention, source-of-truth rule,
  ownership boundary, or previous user-approved canon, stop before editing code
  and ask the user to confirm or change the canon.
- `raw-resource-curator` is the canonical project skill for extracting,
  cataloging, licensing, and converting raw/Fab/Unreal/resource-pack assets.
- `project-resource-adder` is the canonical project skill for copying
  project-ready resources from prepared sources into the external project asset
  store and updating the project asset manifest.
- On a new machine, if the global Codex skill is missing, copy or sync
  `.codex/skills/code-workflow-gate`,
  `.codex/skills/raw-resource-curator`, and
  `.codex/skills/project-resource-adder` into the user's Codex skills directory
  before doing resource work.
- Do not store machine-specific OneDrive paths in tracked skill files or docs;
  ask for the local OneDrive source/project asset paths when needed.
