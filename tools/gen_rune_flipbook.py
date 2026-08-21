"""Generate a 4x4 rune/sigil flipbook for Runecaster cast VFX.

Cyan–gold geometric seals (project-owned; no third-party glyphs).
"""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

OUT = Path(__file__).resolve().parents[1] / "samples/open-world-rpg/assets/vfx"
CELL = 128
COLS = 4
ROWS = 4
FRAMES = COLS * ROWS


def paste_atlas(frames: list[Image.Image], name: str) -> Path:
    atlas = Image.new("RGBA", (CELL * COLS, CELL * ROWS), (0, 0, 0, 0))
    for i, cell in enumerate(frames):
        atlas.paste(cell, ((i % COLS) * CELL, (i // COLS) * CELL), cell)
    out = OUT / name
    atlas.save(out, "PNG")
    print("wrote", out)
    return out


def rune_paths(frame: int) -> list[list[tuple[float, float]]]:
    """Normalized 0..1 polylines that read as different seals across the atlas."""
    t = frame / float(FRAMES)
    # Outer ring points
    ring = []
    n = 6 + (frame % 3)
    for i in range(n):
        a = (i / n) * math.pi * 2.0 + t * math.pi * 2.0
        r = 0.38 + 0.04 * math.sin(a * 3.0 + frame)
        ring.append((0.5 + math.cos(a) * r, 0.5 + math.sin(a) * r))
    ring.append(ring[0])

    # Inner diamond / star
    inner = []
    m = 4 if frame % 2 == 0 else 5
    for i in range(m):
        a = (i / m) * math.pi * 2.0 - t * math.pi
        r = 0.18 + 0.06 * ((frame % 4) / 4.0)
        inner.append((0.5 + math.cos(a) * r, 0.5 + math.sin(a) * r))
    inner.append(inner[0])

    # Cross strokes
    cross_a = [
        (0.5, 0.18),
        (0.5, 0.82),
    ]
    cross_b = [
        (0.18, 0.5),
        (0.82, 0.5),
    ]
    # Diagonal tick that rotates with frame
    a = t * math.pi * 2.0
    tick = [
        (0.5 + math.cos(a) * 0.12, 0.5 + math.sin(a) * 0.12),
        (0.5 + math.cos(a) * 0.34, 0.5 + math.sin(a) * 0.34),
    ]
    return [ring, inner, cross_a, cross_b, tick]


def make_frame(frame: int) -> Image.Image:
    cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    glow = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    draw = ImageDraw.Draw(cell)
    gdraw = ImageDraw.Draw(glow)

    def px(pts: list[tuple[float, float]]) -> list[tuple[float, float]]:
        return [(p[0] * CELL, p[1] * CELL) for p in pts]

    # Soft radial glow disc
    cx, cy = CELL * 0.5, CELL * 0.5
    for r in range(54, 8, -2):
        a = int(18 + (54 - r) * 1.1)
        gdraw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(90, 140, 255, a))

    paths = rune_paths(frame)
    # Gold under-stroke then cyan core
    for path in paths:
        pts = px(path)
        gdraw.line(pts, fill=(255, 210, 120, 90), width=7)
        draw.line(pts, fill=(255, 220, 140, 220), width=5)
        draw.line(pts, fill=(160, 210, 255, 255), width=2)

    # Corner nodes
    for i in range(4):
        a = (i / 4.0) * math.pi * 2.0 + frame * 0.4
        x = cx + math.cos(a) * 42
        y = cy + math.sin(a) * 42
        draw.ellipse([x - 3, y - 3, x + 3, y + 3], fill=(255, 245, 200, 255))
        gdraw.ellipse([x - 6, y - 6, x + 6, y + 6], fill=(120, 180, 255, 70))

    glow = glow.filter(ImageFilter.GaussianBlur(radius=2.2))
    out = Image.alpha_composite(glow, cell)
    # Soft edge falloff
    mask = Image.new("L", (CELL, CELL), 0)
    md = ImageDraw.Draw(mask)
    md.ellipse([10, 10, CELL - 10, CELL - 10], fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(radius=6))
    r, g, b, a = out.split()
    a = ImageChops.multiply(a, mask)
    return Image.merge("RGBA", (r, g, b, a))


def main() -> None:
    frames = [make_frame(i) for i in range(FRAMES)]
    paste_atlas(frames, "rune_sigil_flipbook_4x4.png")


if __name__ == "__main__":
    main()
