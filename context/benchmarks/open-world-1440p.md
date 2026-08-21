# Open-world 1440p / 60 FPS benchmark gate (TICKET-0139)

Date: 2026-08-06 (RelWithDebInfo lock) / 2026-07-23 (Debug baseline)

Reproducible **2560×1440** timing for `samples/open-world-rpg` Game play-test (DEC-0004). This is the ship-oriented gate; [`milestone2.md`](milestone2.md) remains the early triangle smoke baseline only.

## Capture command

```text
engine benchmark --project samples/open-world-rpg [--frames 120] [--world worlds/vertical-slice.world.json] [--report out/benchmarks/open-world-1440p.json] [--json]
```

Defaults: 120 frames, 2560×1440, hidden editor + `debug_world`, Game viewport, auto Start play-test, GPU timestamps required, uncapped present (`Present(0)` — hidden windows stall on vsync). **Default world is `worlds/vertical-slice.world.json`** (not project `defaultWorld` / main-menu — menu Start is preview, not open-world play-test). First 30 frames are warmup (streaming settle); metrics are the remaining frames.

Fail-closed: missing GPU timestamps → `BENCH-GPU-TIMESTAMPS` (no silent pass).

## Environment (RelWithDebInfo lock — this machine)

| Field | Value |
| --- | --- |
| Date | 2026-08-06 |
| GPU | NVIDIA GeForce RTX 2080 SUPER |
| API | Direct3D 12 |
| Build | **RelWithDebInfo** (MSVC windows-msvc-debug tree; report `buildConfig` = `Release` via `NDEBUG`) |
| Scene | `worlds/vertical-slice.world.json`, Game tab + play-test Start, streamed neighborhood |
| Present | Sync interval 0 (uncapped; throughput measurement) |
| Foliage GPU cull | **Enabled** for Scene/run (TICKET-0278); play-test orbit still CPU DrawInstanced |

Raw JSON: [`open-world-1440p-relwithdebinfo.json`](open-world-1440p-relwithdebinfo.json).

## Results (RelWithDebInfo, post-warmup)

| Metric | Value |
| --- | --- |
| Measured frames | 90 (of 120 presented; 30 warmup) |
| Average CPU frame time | **4.51 ms** |
| Average GPU time (timestamps) | **1.31 ms** |
| FPS (from CPU wall) | **221.7** |
| Resident terrain cells | 55 |
| Draw calls (last frame) | 48 |
| Instances drawn (last frame) | 48 |
| GPU timestamps OK | true |

### Reading (RelWithDebInfo)

- On this **2080 SUPER**, RelWithDebInfo clears the provisional 16.67 ms CPU / 60 FPS budgets with large headroom.
- This is **not** the DEC-0004 RTX **4070-class** ship lock — re-run on target GPU before owner locks budgets.
- Do **not** compare to Debug ~50 ms CPU as a ship claim.

## Environment (Debug baseline — historical)

| Field | Value |
| --- | --- |
| Date | 2026-07-23 |
| GPU | NVIDIA GeForce RTX 2080 SUPER |
| Driver | 620.02 |
| Build | **Debug**, MSVC (windows-msvc-debug) — labeled; not a Release ship claim |
| Scene | Game tab + play-test Start, streamed neighborhood |

## Results (Debug, post-warmup) — historical

| Metric | Value |
| --- | --- |
| Measured frames | 90 (of 120 presented; 30 warmup) |
| Average CPU frame time | **50.42 ms** |
| Average GPU time (timestamps) | **1.65 ms** |
| FPS (from CPU wall) | **19.8** |
| Resident terrain cells | 18 |
| Draw calls (last frame) | 23 |
| Instances drawn (last frame) | 3617 |
| GPU timestamps OK | true |

Raw JSON: [`open-world-1440p.json`](open-world-1440p.json).

### Post TICKET-0220 (same harness, 2026-07-23)

| Metric | Baseline (0139) | After 0220 |
| --- | --- | --- |
| Instances drawn (last frame) | 3617 | **2147** (−41%) |
| Draw calls (last frame) | 23 | 26 |
| Average GPU ms | 1.65 | 1.73 |
| Average CPU ms | 50.42 | 56.50 |

Report: `out/benchmarks/open-world-1440p-post-0220.json`. Instance drop is from foliage frustum cull + placed far-mesh cull; draw-call uptick is expected from per-cell×mesh foliage draw splitting.

### Reading (Debug)

- **GPU** is well under a 16.7 ms slice on this card; the Debug editor path is **CPU-bound** (~50 ms).
- Hidden-benchmark teardown hard-exits after writing the report (streamed-field destructors can otherwise stall for minutes); metrics on disk remain authoritative.
- Rapid back-to-back hidden benches after hard-exit can leave DXGI/D3D wedged until a short settle (~5–30s); prefer one gate run or space retries.

## Provisional budgets (owner may lock)

Target machine class: RTX 4070-class desktop (DEC-0004). Frame budget for 60 FPS = **16.67 ms**.

| Budget | Provisional threshold | Notes |
| --- | --- | --- |
| CPU frame time | ≤ **16.67 ms** average | Pass/fail for overland Game play-test at 1440p |
| GPU timestamp time | ≤ **12.0 ms** average | Leaves ~4.5 ms headroom for CSM / post (TICKET-0219+) |
| FPS (derived) | ≥ **60** | From measured CPU wall after warmup |
| Terrain cells | bounded neighborhood (today radius 2 / view bias) | See [`streaming-lod-budgets.md`](../features/streaming-lod-budgets.md) |

Status: **provisional** until owner locks after a labeled Release/RelWithDebInfo capture on the **4070-class** target. 2080 SUPER RelWithDebInfo numbers above are an intermediate labeled capture only.

## Related

- DEC-0004 diagnostics / performance contract
- [`streaming-lod-budgets.md`](../features/streaming-lod-budgets.md)
- [`gpu-instance-cull.md`](../features/gpu-instance-cull.md) (TICKET-0276 — currently fail-closed disabled in draw path)
- TICKET-0139, TICKET-0219 (CSM), TICKET-0220 (cull/LOD), TICKET-0276 (GPU cull)
