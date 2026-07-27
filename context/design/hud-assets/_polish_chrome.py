"""Polish HUD / dialogue chrome PNGs: true alpha, hollow centers, trim fringe.

Writes into both context/design/{hud,dialogue}-assets and samples/.../assets/ui/{hud,dialogue}.
"""
from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

REPO = Path(__file__).resolve().parents[3]
PAIRS = [
    (
        Path(__file__).resolve().parent,
        REPO / "samples" / "open-world-rpg" / "assets" / "ui" / "hud",
        "hud-*.png",
    ),
    (
        REPO / "context" / "design" / "dialogue-assets",
        REPO / "samples" / "open-world-rpg" / "assets" / "ui" / "dialogue",
        "dialogue-*.png",
    ),
]

# Hollow circular rings — punch interior disk.
HOLLOW_CIRCLES = {
    "hud-portrait-ring-hollow.png",
    "hud-portrait-ring.png",
    "hud-minimap-frame.png",
    "dialogue-portrait-ring.png",
}

# Rectangular frames with hollow interior (panel frame).
HOLLOW_FRAMES = {
    "dialogue-panel-frame.png",
}

# Horizontal rail chrome with hollow fill between rails.
HOLLOW_BARS = {
    "hud-resource-bar.png",
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


def checker_mask(rgb: np.ndarray) -> np.ndarray:
    return (chroma(rgb) < 32) & (luma(rgb) > 150)


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


def punch_near_black_border(rgb: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    seed = (alpha > 0) & (chroma(rgb) < 22) & (luma(rgb) < 36)
    return flood_from_border(seed)


def punch_black_background(rgb: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    """Treat near-black low-chroma pixels connected to border as transparent backdrop."""
    seed = (alpha > 0) & (chroma(rgb) < 28) & (luma(rgb) < 42)
    return flood_from_border(seed)


def hollow_circle(alpha: np.ndarray, inner_ratio: float = 0.62) -> np.ndarray:
    h, w = alpha.shape
    yy, xx = np.ogrid[:h, :w]
    cy, cx = (h - 1) * 0.5, (w - 1) * 0.5
    rr = np.sqrt((yy - cy) ** 2 + (xx - cx) ** 2)
    outer = min(h, w) * 0.5
    # Keep ring; clear well inside inner_ratio of outer radius where currently opaque.
    hole = rr < (outer * inner_ratio)
    # Only punch where alpha is currently opaque and roughly circular interior.
    return hole


def hollow_frame_rect(alpha: np.ndarray, inset_frac: float = 0.085) -> np.ndarray:
    h, w = alpha.shape
    top = int(h * inset_frac)
    bot = h - top
    left = int(w * inset_frac * 0.55)  # frames are wider; thinner relative horizontal inset
    right = w - left
    # Slightly thicker inset so gold inner lip remains.
    top = max(top, int(h * 0.10))
    bot = h - max(int(h * 0.10), top)
    left = max(left, int(w * 0.045))
    right = w - left
    mask = np.zeros_like(alpha, dtype=bool)
    mask[top:bot, left:right] = True
    return mask


def hollow_bar_rail(rgb: np.ndarray, alpha: np.ndarray) -> np.ndarray:
    """Clear only near-black fill between rails; keep dark rail chrome."""
    h, w = alpha.shape
    left = int(w * 0.11)
    right = w - left
    top = int(h * 0.36)
    bot = h - top
    band = np.zeros_like(alpha, dtype=bool)
    band[top:bot, left:right] = True
    # Only punch fill pixels that are near-black (not the rail metal).
    return band & (chroma(rgb) < 28) & (luma(rgb) < 48) & (alpha > 0)


def trim_to_content(arr: np.ndarray, pad: int = 4) -> np.ndarray:
    alpha = arr[..., 3]
    ys, xs = np.where(alpha > 8)
    if len(xs) == 0:
        return arr
    y0 = max(0, int(ys.min()) - pad)
    y1 = min(arr.shape[0], int(ys.max()) + 1 + pad)
    x0 = max(0, int(xs.min()) - pad)
    x1 = min(arr.shape[1], int(xs.max()) + 1 + pad)
    return arr[y0:y1, x0:x1]


def polish(path: Path, out_dirs: list[Path]) -> None:
    name = path.name
    im = Image.open(path).convert("RGBA")
    arr = np.array(im)
    rgb = arr[..., :3]
    alpha = arr[..., 3].copy()

    kill = checker_mask(rgb) & (alpha > 0)
    alpha[kill] = 0
    # Resource-bar rails are near-black — only flood-punch from the very dark border.
    if name in HOLLOW_BARS:
        alpha[punch_near_black_border(rgb, alpha)] = 0
    else:
        alpha[punch_black_background(rgb, alpha)] = 0
        alpha[punch_near_black_border(rgb, alpha)] = 0

    if name in HOLLOW_CIRCLES:
        # Portrait with baked face keeps more fill; hollow variants punch deeper.
        if "hollow" in name or name == "dialogue-portrait-ring.png" or name == "hud-minimap-frame.png":
            ratio = 0.58 if "minimap" in name else 0.62
            hole = hollow_circle(alpha, inner_ratio=ratio)
            alpha[hole] = 0
        elif name == "hud-portrait-ring.png":
            # Keep face placeholder but clear outer black fringe only (already done).
            pass

    if name in HOLLOW_FRAMES:
        alpha[hollow_frame_rect(alpha)] = 0

    if name in HOLLOW_BARS:
        # Avoid eating dark rails — only punch near-black fill / backdrop.
        alpha[hollow_bar_rail(rgb, alpha)] = 0

    out = arr.copy()
    out[..., 3] = alpha
    transparent = out[..., 3] == 0
    out[transparent, 0:3] = 0
    out = trim_to_content(out, pad=6)

    result = Image.fromarray(out)
    for d in out_dirs:
        d.mkdir(parents=True, exist_ok=True)
        result.save(d / name, optimize=True)
    print(f"{name}: transparent={transparent.mean():.1%} size={out.shape[1]}x{out.shape[0]}")


ORIGINALS = Path(r"C:\Users\johnr\.cursor\projects\c-Users-johnr-Documents-3DGameIdea\assets")


def main() -> None:
    for design_dir, sample_dir, pattern in PAIRS:
        for path in sorted(design_dir.glob(pattern)):
            src = ORIGINALS / path.name
            if src.exists():
                Image.open(src).convert("RGBA").save(path)
                print(f"restored {path.name}")
            polish(path, [design_dir, sample_dir])


if __name__ == "__main__":
    main()
