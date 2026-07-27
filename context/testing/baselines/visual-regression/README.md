# Visual regression baselines (TICKET-0145)

Dark-fantasy Game play-test PNGs for automated screenshot compare. Captures read the **Game viewport RT** (no ImGui chrome / MCP cursor).

## Cases

| File | Pose |
| --- | --- |
| `game-default.png` | Default orbit after play-test Start |
| `game-look.png` | Same, then look deltas `dx=180`, `dy=40` at frame 20 |

Resolution: **1280×720**. Threshold default: mean absolute RGB **12.0**.

## Refresh baselines (reference GPU)

```text
engine visual-regression --project samples/open-world-rpg --update-baselines [--json]
```

Commit updated PNGs under this directory. Do **not** commit `samples/open-world-rpg/out/visual-regression/` (gitignored).

## Compare

```text
engine visual-regression --project samples/open-world-rpg [--threshold 12] [--json]
```

Exit `0` on pass; `4` (`ValidationFailed`) when mean abs RGB exceeds threshold. Intentional fail check: `--threshold 0.001`.

CTest / `engine test --suite visual_regression` (GPU required). Reference GPU policy: [DEC-0043](../../decisions/index.md#dec-0043-nvidia-reference-gpu-multi-vendor-d3d12-support).
