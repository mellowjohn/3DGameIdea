"""Punch solid black backdrop from Wrathful Conquest title logo → true alpha."""
from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

CONCEPTS = Path(__file__).resolve().parents[2] / "art" / "concepts"
SRC = CONCEPTS / "wrathful-conquest-title-logo.png"
OPAQUE_BACKUP = CONCEPTS / "wrathful-conquest-title-logo.opaque.png"
OUTS = [
    SRC,
    Path(__file__).resolve().parent / "wrathful-conquest-title-logo.png",
]


def flood_near_black(rgb: np.ndarray, luma_max: float = 22.0, chroma_max: float = 14.0) -> np.ndarray:
    """Transparentize near-black pixels connected to the image border."""
    r = rgb[..., 0].astype(np.float32)
    g = rgb[..., 1].astype(np.float32)
    b = rgb[..., 2].astype(np.float32)
    luma = 0.2126 * r + 0.7152 * g + 0.0722 * b
    chroma = np.maximum(np.maximum(r, g), b) - np.minimum(np.minimum(r, g), b)
    seed = (chroma < chroma_max) & (luma < luma_max)

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


def dilate(mask: np.ndarray, iterations: int = 2) -> np.ndarray:
    out = mask.copy()
    for _ in range(iterations):
        padded = np.pad(out, 1, constant_values=False)
        neigh = (
            padded[0:-2, 1:-1]
            | padded[2:, 1:-1]
            | padded[1:-1, 0:-2]
            | padded[1:-1, 2:]
            | padded[0:-2, 0:-2]
            | padded[0:-2, 2:]
            | padded[2:, 0:-2]
            | padded[2:, 2:]
            | out
        )
        out = neigh
    return out


def soft_fringe(kill: np.ndarray, rgb: np.ndarray) -> np.ndarray:
    """Feather alpha near punched edges for dark fringe pixels."""
    dil = dilate(kill, iterations=2)
    r = rgb[..., 0].astype(np.float32)
    g = rgb[..., 1].astype(np.float32)
    b = rgb[..., 2].astype(np.float32)
    luma = 0.2126 * r + 0.7152 * g + 0.0722 * b
    chroma = np.maximum(np.maximum(r, g), b) - np.minimum(np.minimum(r, g), b)
    fringe = dil & ~kill & (luma < 40) & (chroma < 18)
    alpha = np.full(kill.shape, 255, dtype=np.float32)
    alpha[kill] = 0
    alpha[fringe] = np.clip((luma[fringe] - 8.0) / 32.0 * 255.0, 0, 200)
    return alpha.astype(np.uint8)


def punch(path: Path) -> Image.Image:
    im = Image.open(path).convert("RGBA")
    arr = np.array(im)
    rgb = arr[..., :3]
    kill = flood_near_black(rgb)
    # Enclosed letter counters are pure black and not border-connected.
    pure_black = rgb.max(axis=2) <= 2
    kill = kill | pure_black
    alpha = soft_fringe(kill, rgb)

    out = arr.copy()
    out[..., 3] = alpha
    transparent = out[..., 3] == 0
    out[transparent, 0:3] = 0

    ys, xs = np.where(out[..., 3] > 8)
    if len(xs) and len(ys):
        pad = 12
        y0 = max(0, int(ys.min()) - pad)
        y1 = min(out.shape[0], int(ys.max()) + pad + 1)
        x0 = max(0, int(xs.min()) - pad)
        x1 = min(out.shape[1], int(xs.max()) + pad + 1)
        out = out[y0:y1, x0:x1]

    return Image.fromarray(out, mode="RGBA")


def main() -> None:
    if not OPAQUE_BACKUP.exists():
        OPAQUE_BACKUP.write_bytes(SRC.read_bytes())
        print(f"backed up opaque source -> {OPAQUE_BACKUP}")
    source = OPAQUE_BACKUP if OPAQUE_BACKUP.exists() else SRC
    punched = punch(source)
    arr = np.array(punched)
    opaque_ratio = float((arr[..., 3] > 8).mean())
    print(f"opaque coverage={opaque_ratio*100:.1f}% size={punched.size} mode=RGBA")
    for dest in OUTS:
        dest.parent.mkdir(parents=True, exist_ok=True)
        punched.save(dest)
        print(f"wrote {dest}")


if __name__ == "__main__":
    main()
