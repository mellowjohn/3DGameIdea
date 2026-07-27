# TICKET-0145: Dark-fantasy visual regression screenshot tests

- Epic: EPIC-0013
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/39ad3efc569581868201dac36727ea73

## Goal

Add automated (or CLI-driven) **screenshot regression** for the dark-fantasy outdoor look — especially SSAO/lighting stability under look-around — so rendering polish stops depending only on chat-history screenshot bursts.

## Context links

- `context/planning/epics.md` (EPIC-0013)
- `context/art/visual-direction.md`
- `context/testing/findings.md` — SSAO / face-normal flicker entries
- `context/features/mcp-live-editor.md` — `engine_editor_screenshot`, MCP `look`
- Quality/perf audit (owner 2026-07-23): gap #4
- Related: TICKET-0042 (SSAO v1 done), TICKET-0139 (perf gate)

## Acceptance criteria

- [x] Headless or MCP-driven capture of ≥2 fixed Game-view poses (or one pose + scripted look deltas) under `samples/open-world-rpg`.
- [x] Baseline PNGs stored under a documented path (e.g. `samples/open-world-rpg/out/visual-regression/` or `context/testing/baselines/`) with README on how to refresh.
- [x] Comparison path: pixel/mean-abs (or perceptual) diff with a tunable threshold; fail the named suite/CLI with a stable exit code when over threshold.
- [x] At least one case covers **look-around AO stability** (masked UI chrome / MCP cursor excluded from the compare region).
- [x] Named suite or `engine` CLI subcommand documented in Verification; CTest wired when headless-capable.
- [x] Context: `context/testing/coverage.md` + short feature note updated.

## Out of scope

- Full temporal AO history / TAA / FSR (follow-on after this gate exists)
- Photoreal reference matching Unreal Lumen
- UI chrome pixel-perfect ImGui tests
- Video recording pipeline

## Dependencies

- Owner override 2026-07-23: promote from proposed → **active P1**.
- Soft prerequisite: MCP screenshot + `look` already land; no hard block on 0139.

## Verification

Rebuild `engine` if C++ added. Run the regression command/suite; record pass + intentional fail (threshold) in **What changed**. Desktop GPU may be required for capture; prefer headless when possible.

## What changed

### Summary

Shipped `engine visual-regression`: chrome-free Game RT PNG captures (default orbit + look deltas), versioned baselines under `context/testing/baselines/visual-regression/`, mean-abs RGB compare with tunable threshold, CTest/`engine test --suite visual_regression`. Also locked **DEC-0043** (NVIDIA reference GPU, multi-vendor D3D12).

### Files / surfaces

- `include/engine/testing/image_diff.h`, `src/testing/image_diff.cpp`
- `include/engine/rendering/render_app.h` — look/capture-game options; PNG capture; Game RT prefer
- `src/rendering/render_app.cpp` — Game RT capture, PNG write, CLI look apply
- `src/automation/command.cpp` — `visual-regression` harness (spawns child editor captures)
- `src/automation/editor_screenshot.cpp` — `write_rgba_png_path`
- `CMakeLists.txt` + CTest `visual_regression`
- Baselines + README; `context/features/visual-regression.md`; coverage/index
- `context/decisions/index.md` — DEC-0043

### Schema / API

CLI: `engine visual-regression --project … [--update-baselines] [--threshold 12]`. Editor also accepts `--look-dx/--look-dy` and PNG `--output`.

### Verification evidence

- Rebuild Debug OK (C4996 getenv only)
- foliage suite 39/39 (image_diff asserts)
- `--update-baselines` wrote `game-default.png` + `game-look.png`
- Compare pass: exit 0, worstMeanAbsRgb ≈ 1.5 (threshold 12)
- Intentional fail: `--threshold 0.001` → exit 4, `VREG-THRESHOLD`

### Decisions

- Baselines live under versioned `context/testing/` (not gitignored `out/`)
- Child-process captures preserve hidden hard-exit teardown
- Game RT capture excludes ImGui/MCP chrome (no crop mask needed)
- DEC-0043: NVIDIA as reference, not exclusive

### Leftover risk

- Run-to-run GPU nondeterminism (SSAO/noise) can raise meanAbsRgb; threshold 12 absorbs Debug RTX 2080 SUPER noise seen so far
- Max-abs outliers remain high; gate uses mean, not max
- CMake ≥3.25 required to regenerate CTest from lists; Debug tree patched manually for `visual_regression` until next configure

## Agent notes

Ready for owner approval. Quality/perf gap track complete for 0139/0219/0220/0145 pending owner `done`.
