# Open-world 1440p / 60 FPS benchmark gate (TICKET-0139)

Date: 2026-07-23

Reproducible **2560×1440** timing for `samples/open-world-rpg` Game play-test (DEC-0004). This is the ship-oriented gate; [`milestone2.md`](milestone2.md) remains the early triangle smoke baseline only.

## Capture command

```text
engine benchmark --project samples/open-world-rpg [--frames 120] [--report out/benchmarks/open-world-1440p.json] [--json]
```

Defaults: 120 frames, 2560×1440, hidden editor + `debug_world`, Game viewport, auto Start play-test, GPU timestamps required, uncapped present (`Present(0)` — hidden windows stall on vsync). First 30 frames are warmup (streaming settle); metrics are the remaining frames.

Fail-closed: missing GPU timestamps → `BENCH-GPU-TIMESTAMPS` (no silent pass).

Machine-readable dump: [`open-world-1440p.json`](open-world-1440p.json) (copy of the run below).

## Environment (this capture)

| Field | Value |
| --- | --- |
| Date | 2026-07-23 |
| GPU | NVIDIA GeForce RTX 2080 SUPER |
| Driver | 620.02 |
| API | Direct3D 12 |
| Build | **Debug**, MSVC (windows-msvc-debug) — labeled; not a Release ship claim |
| Scene | `samples/open-world-rpg`, Game tab + play-test Start, streamed neighborhood |
| Present | Sync interval 0 (uncapped; throughput measurement) |

## Results (Debug, post-warmup)

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

### Reading

- **GPU** is well under a 16.7 ms slice on this card; the Debug editor path is **CPU-bound** (~50 ms).
- Do **not** treat this Debug run as a pass/fail of the 60 FPS ship gate. Re-run with RelWithDebInfo/Release on an RTX 4070-class machine before locking numbers.
- Hidden-benchmark teardown hard-exits after writing the report (streamed-field destructors can otherwise stall for minutes); metrics on disk remain authoritative.

## Provisional budgets (owner may lock)

Target machine class: RTX 4070-class desktop (DEC-0004). Frame budget for 60 FPS = **16.67 ms**.

| Budget | Provisional threshold | Notes |
| --- | --- | --- |
| CPU frame time | ≤ **16.67 ms** average | Pass/fail for overland Game play-test at 1440p |
| GPU timestamp time | ≤ **12.0 ms** average | Leaves ~4.5 ms headroom for CSM / post (TICKET-0219+) |
| FPS (derived) | ≥ **60** | From measured CPU wall after warmup |
| Terrain cells | bounded neighborhood (today radius 2 / view bias) | See [`streaming-lod-budgets.md`](../features/streaming-lod-budgets.md) |

Status: **provisional** until owner locks after a labeled Release/RelWithDebInfo capture on the target GPU class.

## Related

- DEC-0004 diagnostics / performance contract
- [`streaming-lod-budgets.md`](../features/streaming-lod-budgets.md)
- TICKET-0139, TICKET-0219 (CSM), TICKET-0220 (cull/LOD)
