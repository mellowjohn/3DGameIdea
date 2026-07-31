"""Generate ambient wind streak textures.

Writes:
  - wind_streak.png — single reference frame
  - wind_streak_flipbook_4x4.png — 16-frame subtle morphing atlas
"""
from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "samples" / "open-world-rpg" / "assets" / "vfx"


def draw_streak(
    px,
    origin_x: int,
    origin_y: int,
    cell_w: int,
    cell_h: int,
    phase: float,
    seed: float,
) -> None:
    """Soft bright ribbon with very gentle phase drift — hint of motion, not a whip."""
    # Near-flat curves; morph comes mostly from phase + soft highlight travel.
    amp1 = 0.022 + 0.012 * (0.5 + 0.5 * math.sin(phase * 0.7 + seed))
    amp2 = 0.006 + 0.006 * (0.5 + 0.5 * math.cos(phase * 0.9 - seed))
    thick = 2.6 + 0.35 * (0.5 + 0.5 * math.sin(phase * 1.1 + seed * 0.4))
    glow_r = 11.0 + 2.0 * (0.5 + 0.5 * math.sin(phase * 0.6 + seed * 0.2))
    ghost_on = 0.1 + 0.12 * max(0.0, math.sin(phase * 1.4 + seed))
    ghost_shift = 1.0 + 0.7 * math.sin(phase * 0.5 + seed)
    core_t = (phase / (math.pi * 2.0)) % 1.0

    def curve_y(t: float) -> float:
        return (
            0.5
            + amp1 * math.sin(t * math.pi * 2.0 + phase)
            + amp2 * math.sin(t * math.pi * 3.0 - phase * 0.8 + seed)
        )

    for lx in range(cell_w):
        t = lx / max(cell_w - 1, 1)
        edge = min(t, 1.0 - t)
        taper = max(0.0, min(1.0, edge / 0.14))
        taper = taper * taper * (3 - 2 * taper)
        along = math.exp(-((t - core_t) * 3.2) ** 2)
        # Soft traveling highlight — never drops the body too dark.
        pulse = 0.82 + 0.18 * along
        cy = curve_y(t) * (cell_h - 1)
        for ly in range(cell_h):
            dy = abs(ly - cy)
            core = math.exp(-(dy * dy) / (thick * thick))
            glow = math.exp(-(dy * dy) / (glow_r * glow_r))
            wisp = 0.28 * math.exp(-((dy - 2.2) ** 2) / (2.0 * 2.0)) + 0.18 * math.exp(
                -((dy + 1.8) ** 2) / (2.2 * 2.2)
            )
            a = (1.0 * core + 0.48 * glow + 0.16 * wisp) * taper * pulse

            if ghost_on > 0.05:
                gy = cy + ghost_shift * math.sin(t * math.pi + phase * 0.5)
                gdy = abs(ly - gy)
                g_core = math.exp(-(gdy * gdy) / ((thick * 0.75) ** 2))
                a += 0.2 * ghost_on * g_core * taper * pulse

            a = max(0.0, min(1.0, a))
            if a < 0.008:
                continue
            # Hot white core — alpha blend multiplies color, so start bright.
            r = 255
            g = 255
            b = 255
            x = origin_x + lx
            y = origin_y + ly
            prev = px[x, y]
            if prev[3] > 0:
                a = max(a, prev[3] / 255.0)
                r = max(r, prev[0])
                g = max(g, prev[1])
                b = max(b, prev[2])
            px[x, y] = (min(255, r), min(255, g), min(255, b), int(a * 255))


def make_single() -> Path:
    w, h = 512, 128
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw_streak(img.load(), 0, 0, w, h, phase=0.35, seed=0.2)
    img = img.filter(ImageFilter.GaussianBlur(radius=1.0))
    out = OUT_DIR / "wind_streak.png"
    img.save(out, "PNG")
    return out


def make_flipbook() -> Path:
    atlas = 512
    cols = 4
    cell = atlas // cols
    img = Image.new("RGBA", (atlas, atlas), (0, 0, 0, 0))
    px = img.load()
    frames = cols * cols
    for i in range(frames):
        row, col = divmod(i, cols)
        phase = (i / frames) * math.pi * 2.0
        seed = 0.15 + i * 0.31
        draw_streak(px, col * cell, row * cell, cell, cell, phase=phase, seed=seed)
    img = img.filter(ImageFilter.GaussianBlur(radius=0.85))
    out = OUT_DIR / "wind_streak_flipbook_4x4.png"
    img.save(out, "PNG")
    return out


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    single = make_single()
    flip = make_flipbook()
    print("wrote", single, Image.open(single).size)
    print("wrote", flip, Image.open(flip).size)


if __name__ == "__main__":
    main()
