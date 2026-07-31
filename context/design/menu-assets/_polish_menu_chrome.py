"""Polish menu/creation chrome PNGs: true alpha, hollow wells, optional Pencil mattes.

Source originals: Cursor assets/ copies when present, else current menu-assets files.
Writes transparent PNGs into context/design/menu-assets/.
Also writes iron-matted copies under menu-assets/pencil-matte/ for Pencil fills
(Pencil image fills do not composite PNG alpha).
"""
from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent
ORIGINALS = Path(r"C:\Users\johnr\.cursor\projects\c-Users-johnr-Documents-3DGameIdea\assets")
MATTE_DIR = ROOT / "pencil-matte"
IRON = (45, 41, 35, 255)  # #2D2923
SCRIM = (26, 24, 20, 255)  # #1A1814

FILES = [
    "menu-panel-frame.png",
    "menu-title-plate.png",
    "menu-btn-primary.png",
    "menu-btn-secondary.png",
    "menu-archetype-card.png",
    "icon-ashfell-blade.png",
    "icon-outrider.png",
    "icon-runecaster.png",
]

HOLLOW_FRAMES = {
    # Keep solid plates for Pencil/engine panel fills. Side rails were parchment-only
    # in the AI gen; hollowing left only top/bottom bars.
}


def luma(rgb: np.ndarray) -> np.ndarray:
    r = rgb[..., 0].astype(np.float32)
    g = rgb[..., 1].astype(np.float32)
    b = rgb[..., 2].astype(np.float32)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def chroma(rgb: np.ndarray) -> np.ndarray:
    r = rgb[..., 0].astype(np.float32)
    g = rgb[..., 1].astype(np.float32)
    b = rgb[..., 2].astype(np.float32)
    return np.maximum(np.maximum(r, g), b) - np.minimum(np.minimum(r, g), b)


def flood_from_border(seed: np.ndarray) -> np.ndarray:
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


def light_backdrop_seed(rgb: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    # Near-white / light gray studio backdrop (AI gens often bake this).
    return (alpha > 0) & (chroma(rgb) < 28) & (luma(rgb) > 200)


def black_backdrop_seed(rgb: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    return (alpha > 0) & (chroma(rgb) < 28) & (luma(rgb) < 42)


def hollow_frame_rect(alpha: np.ndarray, inset_frac: float) -> np.ndarray:
    h, w = alpha.shape
    top = max(int(h * inset_frac), int(h * 0.08))
    bot = h - top
    left = max(int(w * inset_frac), int(w * 0.08))
    right = w - left
    mask = np.zeros_like(alpha, dtype=bool)
    mask[top:bot, left:right] = True
    return mask


def trim_to_content(arr: np.ndarray, pad: int = 6) -> np.ndarray:
    alpha = arr[..., 3]
    ys, xs = np.where(alpha > 8)
    if len(xs) == 0:
        return arr
    y0 = max(0, int(ys.min()) - pad)
    y1 = min(arr.shape[0], int(ys.max()) + 1 + pad)
    x0 = max(0, int(xs.min()) - pad)
    x1 = min(arr.shape[1], int(xs.max()) + 1 + pad)
    return arr[y0:y1, x0:x1]


def polish_one(name: str) -> None:
    src = ORIGINALS / name
    cur = ROOT / name
    if src.exists():
        im = Image.open(src).convert("RGBA")
    elif cur.exists():
        im = Image.open(cur).convert("RGBA")
    else:
        print(f"skip missing {name}")
        return

    arr = np.array(im)
    rgb = arr[..., :3]
    alpha = arr[..., 3].copy()

    alpha[flood_from_border(light_backdrop_seed(rgb, alpha))] = 0
    alpha[flood_from_border(black_backdrop_seed(rgb, alpha))] = 0
    # Also punch any remaining near-white islands (checker cells not border-connected).
    alpha[(chroma(rgb) < 24) & (luma(rgb) > 220) & (alpha > 0)] = 0

    if name in HOLLOW_FRAMES:
        # Only hollow where currently opaque (keep outer chrome).
        hole = hollow_frame_rect(alpha, HOLLOW_FRAMES[name])
        alpha[hole & (alpha > 0)] = 0

    out = arr.copy()
    out[..., 3] = alpha
    transparent = out[..., 3] == 0
    out[transparent, 0:3] = 0
    out = trim_to_content(out, pad=8)

    result = Image.fromarray(out, mode="RGBA")
    ROOT.mkdir(parents=True, exist_ok=True)
    result.save(ROOT / name, optimize=True)

    # Pencil-safe matte: composite over iron so fills don't flash white.
    matte_color = SCRIM if name.startswith("icon-") else IRON
    base = Image.new("RGBA", result.size, matte_color)
    matted = Image.alpha_composite(base, result)
    MATTE_DIR.mkdir(parents=True, exist_ok=True)
    matted.convert("RGB").save(MATTE_DIR / name, optimize=True)

    print(
        f"{name}: transparent={float(transparent.mean()):.1%} "
        f"size={out.shape[1]}x{out.shape[0]}"
    )


def main() -> None:
    for name in FILES:
        polish_one(name)


if __name__ == "__main__":
    main()
