# Visual regression (Game viewport screenshots)

Status: needs-approval (TICKET-0145)

Headless **Game play-test** PNG captures compared to versioned baselines for dark-fantasy outdoor look / look-around AO stability.

## Command

```text
engine visual-regression --project samples/open-world-rpg [--update-baselines] [--threshold 12] [--frames 40] [--json]
```

- Captures spawn child `engine editor --viewport game --hidden --output …png` processes (hard-exit safe).
- Shots: `game-default` (default orbit), `game-look` (`--look-dx 180 --look-dy 40`).
- Compare: mean absolute RGB vs `context/testing/baselines/visual-regression/` (chrome-free Game RT).
- Exit `0` pass; `4` (`ValidationFailed`) over threshold.
- CTest / `engine test --suite visual_regression` (GPU required).

Refresh + case table: [`../testing/baselines/visual-regression/README.md`](../testing/baselines/visual-regression/README.md). Reference GPU: [DEC-0043](../decisions/index.md#dec-0043-nvidia-reference-gpu-multi-vendor-d3d12-support).
