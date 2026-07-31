"""Ingest Cursor AI concept icons: punch black/checkerboard to true alpha, resize, sync, matte."""
from __future__ import annotations

from pathlib import Path

import numpy as np
from PIL import Image

REPO = Path(__file__).resolve().parents[3]
SRC_CANDIDATES = [
    Path(__file__).resolve().parent / "ai-source",
    Path.home() / ".cursor" / "projects" / "c-Users-johnr-Documents-3DGameIdea" / "assets",
]
ROOT = Path(__file__).resolve().parent
RUNTIME = REPO / "samples" / "open-world-rpg" / "assets" / "ui" / "quest"
MATTE = ROOT / "pencil-matte"
IRON = (30, 28, 24, 255)

AI_MAP = {
    "quest-mark-available-ai.png": ("quest-mark-available.png", 256),
    "quest-mark-turnin-ai.png": ("quest-mark-turnin.png", 256),
    "quest-pin-main-ai.png": ("quest-pin-main.png", 256),
    "quest-pin-side-ai.png": ("quest-pin-side.png", 256),
    "quest-pin-faction-ai.png": ("quest-pin-faction.png", 256),
    "quest-pin-archetype-ai.png": ("quest-pin-archetype.png", 256),
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


def find_src(name: str) -> Path | None:
    for base in SRC_CANDIDATES:
        p = base / name
        if p.exists():
            return p
    return None


def punch_backdrop_no_scipy(img: Image.Image) -> Image.Image:
    rgba = np.array(img.convert("RGBA"))
    rgb = rgba[..., :3]
    a = rgba[..., 3].astype(np.float32)
    L = luma(rgb)
    C = chroma(rgb)
    seed = (C < 36) & ((L < 48) | (L > 140))
    backdrop = flood_from_border(seed)
    a[backdrop] = 0
    # Manual 3x3 near-clear soften
    clear = a == 0
    h, w = clear.shape
    near = np.zeros_like(clear)
    near[:-1, :] |= clear[1:, :]
    near[1:, :] |= clear[:-1, :]
    near[:, :-1] |= clear[:, 1:]
    near[:, 1:] |= clear[:, :-1]
    fringe = (~backdrop) & near & (C < 45) & ((L < 80) | (L > 130))
    a[fringe] *= 0.2
    rgba[..., 3] = np.clip(a, 0, 255).astype(np.uint8)
    return Image.fromarray(rgba, "RGBA")


def trim_content(img: Image.Image, pad: int = 8) -> Image.Image:
    a = np.array(img.split()[-1])
    ys, xs = np.where(a > 8)
    if len(xs) == 0:
        return img
    x0, x1 = max(0, xs.min() - pad), min(img.width, xs.max() + pad + 1)
    y0, y1 = max(0, ys.min() - pad), min(img.height, ys.max() + pad + 1)
    return img.crop((x0, y0, x1, y1))


def fit_square(img: Image.Image, size: int) -> Image.Image:
    img = trim_content(img, pad=16)
    w, h = img.size
    scale = min(size / w, size / h) * 0.90
    nw, nh = max(1, int(w * scale)), max(1, int(h * scale))
    resized = img.resize((nw, nh), Image.Resampling.LANCZOS)
    out = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    out.paste(resized, ((size - nw) // 2, (size - nh) // 2), resized)
    return out


def write_matte(img: Image.Image, name: str) -> None:
    MATTE.mkdir(parents=True, exist_ok=True)
    bg = Image.new("RGBA", img.size, IRON)
    bg.alpha_composite(img)
    bg.convert("RGB").save(MATTE / name, "PNG")


def hollow_iron_well(img: Image.Image) -> Image.Image:
    """Punch remaining dark iron fill so marks are frame+glyph on true alpha."""
    rgba = np.array(img.convert("RGBA"))
    rgb = rgba[..., :3]
    a = rgba[..., 3].astype(np.float32)
    iron = (a > 0) & (chroma(rgb) < 45) & (luma(rgb) < 70)
    a[iron] = 0
    rgb[a < 1] = 0
    rgba[..., :3] = rgb
    rgba[..., 3] = np.clip(a, 0, 255).astype(np.uint8)
    return Image.fromarray(rgba, "RGBA")


def main() -> None:
    ROOT.mkdir(parents=True, exist_ok=True)
    RUNTIME.mkdir(parents=True, exist_ok=True)
    for src_name, (out_name, size) in AI_MAP.items():
        src = find_src(src_name)
        if src is None:
            print("MISSING", src_name)
            continue
        polished = fit_square(punch_backdrop_no_scipy(Image.open(src)), size)
        if out_name.startswith("quest-mark-"):
            polished = hollow_iron_well(polished)
        # Ensure RGB is zero under transparent texels
        arr = np.array(polished)
        arr[arr[..., 3] < 1, :3] = 0
        polished = Image.fromarray(arr, "RGBA")
        for d in (ROOT, RUNTIME):
            polished.save(d / out_name, "PNG")
        write_matte(polished, out_name)
        print("wrote", out_name, "from", src.parent.name)


if __name__ == "__main__":
    main()
