"""Remove baked checkerboard backgrounds from dialogue PNGs → true alpha."""
from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
OUT_DIRS = [
    ROOT,
    Path(__file__).resolve().parents[3]
    / "samples"
    / "open-world-rpg"
    / "assets"
    / "ui"
    / "dialogue",
]


def checker_mask(rgb: np.ndarray) -> np.ndarray:
    r = rgb[..., 0].astype(np.float32)
    g = rgb[..., 1].astype(np.float32)
    b = rgb[..., 2].astype(np.float32)
    mx = np.maximum(np.maximum(r, g), b)
    mn = np.minimum(np.minimum(r, g), b)
    chroma = mx - mn
    luma = 0.2126 * r + 0.7152 * g + 0.0722 * b
    return (chroma < 32) & (luma > 150)


def near_black_border(rgb: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    r = rgb[..., 0].astype(np.float32)
    g = rgb[..., 1].astype(np.float32)
    b = rgb[..., 2].astype(np.float32)
    luma = 0.2126 * r + 0.7152 * g + 0.0722 * b
    chroma = np.maximum(np.maximum(r, g), b) - np.minimum(np.minimum(r, g), b)
    seed = (alpha > 0) & (chroma < 18) & (luma < 28)

    h, w = seed.shape
    visited = np.zeros((h, w), dtype=bool)
    stack: list[tuple[int, int]] = []
    for x in range(w):
        if seed[0, x]:
            stack.append((0, x))
        if seed[h - 1, x]:
            stack.append((h - 1, x))
    for y in range(h):
        if seed[y, 0]:
            stack.append((y, 0))
        if seed[y, w - 1]:
            stack.append((y, w - 1))
    while stack:
        y, x = stack.pop()
        if y < 0 or y >= h or x < 0 or x >= w or visited[y, x] or not seed[y, x]:
            continue
        visited[y, x] = True
        stack.extend(((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)))
    return visited


def clean_image(path: Path) -> None:
    im = Image.open(path).convert("RGBA")
    arr = np.array(im)
    rgb = arr[..., :3]
    alpha = arr[..., 3].copy()

    kill = checker_mask(rgb) & (alpha > 0)
    alpha[kill] = 0
    alpha[near_black_border(rgb, alpha)] = 0

    out = arr.copy()
    out[..., 3] = alpha
    transparent = out[..., 3] == 0
    out[transparent, 0:3] = 0

    result = Image.fromarray(out)
    for d in OUT_DIRS:
        d.mkdir(parents=True, exist_ok=True)
        result.save(d / path.name, optimize=True)
    print(f"{path.name}: transparent={transparent.mean():.1%}")


def main() -> None:
    originals = Path(r"C:\Users\johnr\.cursor\projects\c-Users-johnr-Documents-3DGameIdea\assets")
    for path in sorted(ROOT.glob("dialogue-*.png")):
        src = originals / path.name
        if src.exists():
            Image.open(src).convert("RGBA").save(path)
            print(f"restored {path.name} from original")
        clean_image(path)


if __name__ == "__main__":
    main()
