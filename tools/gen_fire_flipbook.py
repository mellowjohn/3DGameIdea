"""Generate stylized campfire VFX atlases (flame, smoke, ember).

Flame cells are short jagged tongues (not full-height columns) so many
billboards stack into a dense campfire instead of looking like vertical sheets.
"""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageFilter

OUT = Path(__file__).resolve().parents[1] / "samples/open-world-rpg/assets/vfx"
CELL = 128
COLS = 4
ROWS = 4
FRAMES = COLS * ROWS
PAD = 8


def fract(x: float) -> float:
    return x - math.floor(x)


def hash2(x: float, y: float) -> float:
    return fract(math.sin(x * 127.1 + y * 311.7) * 43758.5453)


def smooth_noise(x: float, y: float) -> float:
    x0, y0 = math.floor(x), math.floor(y)
    fx, fy = fract(x), fract(y)
    fx = fx * fx * (3.0 - 2.0 * fx)
    fy = fy * fy * (3.0 - 2.0 * fy)
    return (
        (hash2(x0, y0) * (1 - fx) + hash2(x0 + 1, y0) * fx) * (1 - fy)
        + (hash2(x0, y0 + 1) * (1 - fx) + hash2(x0 + 1, y0 + 1) * fx) * fy
    )


def fbm(x: float, y: float, octaves: int = 4) -> float:
    amp, freq, total, norm = 0.5, 1.0, 0.0, 0.0
    for _ in range(octaves):
        total += amp * smooth_noise(x * freq, y * freq)
        norm += amp
        amp *= 0.5
        freq *= 2.0
    return total / max(norm, 1e-6)


def border(x: int, y: int) -> float:
    edge = min(x, CELL - 1 - x, y, CELL - 1 - y)
    if edge >= PAD:
        return 1.0
    t = edge / float(PAD)
    return t * t * (3.0 - 2.0 * t)


def paste_atlas(frames: list[Image.Image], name: str) -> Path:
    atlas = Image.new("RGBA", (CELL * COLS, CELL * ROWS), (0, 0, 0, 0))
    for i, cell in enumerate(frames):
        atlas.paste(cell, ((i % COLS) * CELL, (i // COLS) * CELL), cell)
    out = OUT / name
    atlas.save(out, "PNG")
    print("wrote", out)
    return out


def make_flame_frame(phase: float) -> Image.Image:
    """Short wide jagged teardrop — chunky tongues that stack into a fire volume."""
    cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    px = cell.load()
    ang = phase * math.pi * 2.0
    lean = 0.12 * math.sin(ang * 1.7 + 0.4)
    # Keep aspect close to square so billboards aren't tall needles.
    for y in range(CELL):
        for x in range(CELL):
            u = (x + 0.5) / CELL
            v = 1.0 - (y + 0.5) / CELL
            n = fbm(u * 6.0 + math.cos(ang), v * 6.0 + math.sin(ang), 4)
            crunch = fbm(u * 18.0 + math.cos(ang * 2), v * 18.0 - math.sin(ang * 2), 3)
            # Map into a fat teardrop centered in the cell.
            wu = (u - 0.5 - lean * (v - 0.35)) / 0.36 + (n - 0.5) * 0.28
            wv = (v - 0.22) / 0.55 + (crunch - 0.5) * 0.1
            if wv < -0.05 or wv > 1.2:
                continue
            # Wide base, pointed tip — painterly / middle-ref silhouette.
            half_w = 0.95 * (max(0.0, 1.0 - wv * 0.85) ** 0.7)
            half_w *= 1.0 + (crunch - 0.5) * 0.7
            half_w = max(0.06, float(half_w.real) if isinstance(half_w, complex) else float(half_w))
            dx = abs(wu)
            body = max(0.0, 1.0 - dx / half_w)
            body = body ** 1.15
            if wv < 0.0:
                tip_falloff = max(0.0, 1.0 - abs(wv - 0.35) * 1.1)
            else:
                tip_falloff = max(0.0, 1.0 - wv) ** 0.25
            body *= tip_falloff
            body *= max(0.0, 1.0 - max(0.0, -wv) * 4.0)
            forks = 0.0
            for k in range(4):
                seed = hash2(k * 11.0 + phase * 3.0, 7.0)
                fx = (seed - 0.5) * 1.1 * (0.25 + wv)
                fh = 0.25 + 0.55 * seed
                forks += (
                    0.65
                    * max(0.0, 1.0 - abs(wu - fx) / 0.22)
                    * max(0.0, 1.0 - abs(wv - fh) / 0.32)
                    * (0.55 + 0.45 * math.sin(ang + seed * 6.0))
                )
            a = (0.95 * body + 0.6 * forks) * (0.65 + 0.35 * n)
            a *= border(x, y)
            a = max(0.0, min(1.0, a * 1.05))
            if a < 0.03:
                continue
            heat = max(0.0, min(1.0, 1.1 - wv * 0.9 + 0.1 * n))
            r = 1.0
            g = 0.2 + 0.75 * (heat ** 1.1)
            b = 0.02 + 0.42 * (heat ** 2.5)
            if heat > 0.7:
                w = (heat - 0.7) / 0.3
                g = g * (1 - w) + 0.98 * w
                b = b * (1 - w) + 0.85 * w
            px[x, y] = (int(r * 255), int(min(1, g) * 255), int(min(1, b) * 255), int(a * 255))
    return cell.filter(ImageFilter.GaussianBlur(radius=0.4))


def make_smoke_frame(phase: float) -> Image.Image:
    """Soft purple-brown curls for a readable plume."""
    cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    px = cell.load()
    ang = phase * math.pi * 2.0
    cx_s, sy = math.cos(ang), math.sin(ang)
    for y in range(CELL):
        for x in range(CELL):
            u = (x + 0.5) / CELL
            v = 1.0 - (y + 0.5) / CELL
            n1 = fbm(u * 3.2 + cx_s, v * 3.2 + sy, 4)
            n2 = fbm(u * 3.2 - sy * 1.2, v * 3.2 + cx_s * 1.2, 4)
            wu = u + (n1 - 0.5) * 0.38
            wv = v + (n2 - 0.5) * 0.28
            swirl = fbm(wu * 5.5 + cx_s * 2.0, wv * 5.5 + sy * 2.0, 5)
            blob = math.exp(-(((wu - 0.5) / 0.4) ** 2 + ((wv - 0.48) / 0.4) ** 2))
            ribbon = math.exp(-((wu - (0.5 + 0.18 * sy + (swirl - 0.5) * 0.45)) / 0.2) ** 2)
            ribbon *= max(0.0, 1.0 - abs(wv - 0.52) * 1.35)
            a = (0.7 * blob + 0.8 * ribbon) * (0.4 + 0.6 * swirl)
            a *= 0.4 + 0.6 * v
            a *= border(x, y)
            a = max(0.0, min(1.0, a * 0.95))
            if a < 0.02:
                continue
            r = 0.34 + 0.14 * swirl
            g = 0.14 + 0.08 * swirl
            b = 0.2 + 0.1 * (1.0 - swirl)
            px[x, y] = (int(r * 255), int(g * 255), int(b * 255), int(a * 255))
    return cell.filter(ImageFilter.GaussianBlur(radius=1.15))


def make_ember_frame(phase: float) -> Image.Image:
    """One bright stylized spark per cell (readable at campfire scale)."""
    cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    px = cell.load()
    seed = hash2(phase * 17.0, 4.0)
    sx = 0.5 + 0.12 * math.sin(phase * math.pi * 2.0 + seed)
    sy = 0.48 + 0.1 * math.cos(phase * math.pi * 2.0 * 1.3)
    rad = 0.09 + 0.03 * seed
    for y in range(CELL):
        for x in range(CELL):
            u = (x + 0.5) / CELL
            v = 1.0 - (y + 0.5) / CELL
            dx, dy = u - sx, v - sy
            # Soft core + slightly square fleck (middle-ref sparks)
            d = math.sqrt(dx * dx + dy * dy)
            d2 = max(abs(dx), abs(dy))
            soft = math.exp(-(d / rad) ** 2)
            hard = max(0.0, 1.0 - d2 / (rad * 1.15)) ** 1.4
            a = 0.55 * soft + 0.7 * hard
            # Tiny glow halo
            a = max(a, 0.25 * math.exp(-(d / (rad * 2.2)) ** 2))
            a *= border(x, y)
            if a < 0.04:
                continue
            heat = 0.75 + 0.25 * seed
            r, g, b = 1.0, 0.55 + 0.4 * heat, 0.1 + 0.35 * heat
            px[x, y] = (int(r * 255), int(g * 255), int(b * 255), int(min(1.0, a) * 255))
    return cell.filter(ImageFilter.GaussianBlur(radius=0.5))


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    paste_atlas([make_flame_frame(i / FRAMES) for i in range(FRAMES)], "fire_flipbook_4x4.png")
    paste_atlas([make_smoke_frame(i / FRAMES) for i in range(FRAMES)], "smoke_flipbook_4x4.png")
    paste_atlas([make_ember_frame(i / FRAMES) for i in range(FRAMES)], "ember_flipbook_4x4.png")


if __name__ == "__main__":
    main()
