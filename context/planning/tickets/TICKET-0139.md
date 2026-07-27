# TICKET-0139: 1440p/60 FPS benchmark gate + GPU budgets

- Epic: EPIC-0012
- Status: needs-approval
- Agent: cursor-agent
- Priority: P1
- Notion: https://app.notion.com/p/39ad3efc5695810b84f1fdfc75a0deb3

## Goal

Establish a reproducible **1440p / 60 FPS** performance gate on an RTX 4070-class PC (DEC-0004) with published CPU/GPU frame-time budgets for a representative streamed open-world scene, so lighting/LOD/content changes can be judged against numbers instead of vibes.

## Context links

- `context/planning/epics.md` (EPIC-0012)
- [DEC-0004](../../decisions/index.md#dec-0004-diagnostics-and-performance-contract) — 1440p/60 target
- `context/features/streaming-lod-budgets.md` — resident-set policy (TICKET-0032)
- `context/benchmarks/milestone2.md` — M2 triangle baseline (not a ship gate)
- `context/benchmarks/open-world-1440p.md` — **gate doc + provisional budgets**
- Quality/perf audit (owner 2026-07-23): gap #1
- Soft blocked by / informs: TICKET-0219 (CSM), TICKET-0220 (cull/LOD)

## Acceptance criteria

- [x] Documented benchmark scene: `samples/open-world-rpg` Game play-test pose at **2560×1440**, streamed neighborhood loaded, foliage + placements present.
- [x] CLI path captures average **CPU ms**, **GPU ms** (timestamp queries), FPS, resident terrain cell count, and draw/instance counts over ≥60 frames (Debug labeled; Release preferred later).
- [x] Results written under `context/benchmarks/` with GPU adapter, driver, build config, and date.
- [x] Provisional GPU/CPU budgets recorded for overland play (pass/fail thresholds); marked provisional until owner locks numbers.
- [x] Feature index / streaming-lod-budgets cross-link updated to point at the new benchmark doc.
- [x] Fail-closed: missing GPU timestamps returns `BENCH-GPU-TIMESTAMPS` (no silent pass).

## Out of scope

- Shipping a packaged player build (TICKET-0142)
- Implementing CSM, LOD, or TAA in this ticket
- Multi-platform budgets (Windows desktop first)
- Guaranteeing 60 FPS in Debug editor with ImGui dual viewports

## Dependencies

- Owner override 2026-07-23: promote from P3/proposed → **active P1** (quality/perf gap track).
- Parallel OK with TICKET-0219 / 0220 / 0145; prefer landing the measurement harness before claiming those tickets “improve FPS.”

## Verification

Rebuild `engine` (Debug). Run:

`build/windows-msvc-debug/Debug/engine.exe benchmark --project samples/open-world-rpg --json --report out/benchmarks/open-world-1440p.json`

Exit 0 in ~20s; report + `context/benchmarks/open-world-1440p.md` updated. Desktop GPU required.

## What changed

### Summary

Shipped the DEC-0004 **1440p benchmark CLI gate**: `engine benchmark` defaults to 2560×1440 / 120 frames / hidden editor+debug_world / Game play-test Start / required GPU timestamps / JSON report. Fixed hidden-window Present stall (uncapped present), warmup frame exclusion, single Game world pass for measurement, fail-closed timestamp errors, and hard-exit after durable report (streamer teardown otherwise hung). Published provisional CPU/GPU budgets and a labeled Debug capture on RTX 2080 SUPER.

### Files / surfaces

- `include/engine/rendering/render_app.h` — `require_gpu_timestamps`, `benchmark_report_path`, `cli_json`; richer `RenderStats`
- `src/rendering/render_app.cpp` — counters, report JSON, play-test auto-start, Present(0) when hidden, warmup metrics, Game-only pass for bench, teardown/`_Exit` for hidden bench
- `src/automation/command.cpp` — benchmark defaults, help, metrics wiring
- `context/benchmarks/open-world-1440p.md` + `.json`
- `context/features/streaming-lod-budgets.md`, `context/features/index.md`
- `context/planning/epics.md`, this stub

### Schema / API

- CLI: `engine benchmark --project <path> [--frames N] [--report path] [--json]`
- Report JSON schemaVersion 1 (adapter, cpu/gpu ms, fps, terrainCells, drawCalls, instances, warmupFrames, buildConfig, …)
- Diagnostic: `BENCH-GPU-TIMESTAMPS`

### Samples

Uses existing `samples/open-world-rpg` (no sample content change).

### Verification evidence

- Rebuild `engine` Debug: OK (warning C4996 getenv only)
- Run 2026-07-23: FPS ~19.8, CPU ~50.4 ms, GPU ~1.65 ms, terrainCells 18, drawCalls 23, instances 3617, gpuTimestampsOk true; exit 0 in ~19s
- Adapter NVIDIA GeForce RTX 2080 SUPER, driver 620.02, build Debug

### Decisions

- Measure uncapped present for hidden runs (vsync stalls invisible windows).
- Provisional budgets: CPU ≤16.67 ms, GPU ≤12 ms, FPS ≥60 at 1440p (owner lock pending Release on 4070-class).
- Debug capture labeled; not a ship pass.

### Leftover risk

- Release/RelWithDebInfo re-capture on target GPU still needed before locking budgets.
- Hidden-bench hard-exit skips normal atexit (report + CLI summary already emitted).
- CPU bound in Debug editor path; GPU headroom should not be read as “done” for CSM/LOD work.

## Agent notes

Owner activated from competitive quality/perf audit 2026-07-23. Next in gap track: TICKET-0219 CSM.
