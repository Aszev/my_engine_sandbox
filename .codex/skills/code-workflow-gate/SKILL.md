---
name: code-workflow-gate
description: Use for every task that reads, changes, reviews, builds, tests, or reasons about source code in this project. This gate requires choosing and loading the most specific applicable Codex skill before code work; default to architecture-audit-first for engine, editor, runtime, plugin, ECS, input, camera, rendering, prefab, settings, build, or asset-pipeline changes.
---

# Code Workflow Gate

Before any code work:

1. Select the most specific applicable skill and load it before editing or reviewing code.
2. Use `architecture-audit-first` for engine/editor/runtime/plugin/ECS/input/camera/rendering/prefab/settings/build/asset-pipeline changes.
3. Use `project-resource-adder` for adding project resources and `raw-resource-curator` for raw/source asset extraction or cataloging.
4. If no narrower skill applies, use this skill as the minimum workflow gate.
5. Announce the skill choice briefly to the user.

Canon doubt stop rule:

- If the current fix seems to contradict an architecture document, named
  convention, source-of-truth rule, coordinate/sign convention, ownership
  boundary, or previous user-approved design, stop before editing code.
- Do not patch around the conflict locally.
- Summarize the conflict in 2-5 lines and ask the user to confirm the canon or
  choose a new one.
- Continue implementation only after the canon is confirmed or updated.

Minimum workflow when this is the only applicable skill:

1. Inspect the relevant files and current behavior.
2. Identify the intended owner/layer for the change.
3. Make the smallest coherent change that fits the existing architecture.
4. Validate with the nearest build, test, or smoke run.
5. Report changed files, validation, and any remaining risk.

Never bypass project instructions, dirty-worktree safety, or validation just because a change looks small.
