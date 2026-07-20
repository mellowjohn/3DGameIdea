#!/usr/bin/env python3
"""Generate transparent cartography stroke tiles (borders / roads / ferry / river).

Produces project-owned PNG stamps with alpha. Mountains are intentionally omitted —
they stay in discrete map plates. Runtime copies live under samples/.../cartography/strokes/.
"""
from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
CONTEXT = ROOT / "context" / "art" / "cartography" / "strokes"
RUNTIME = ROOT / "samples" / "open-world-rpg" / "assets" / "ui" / "cartography" / "strokes"

# Tile size: horizontal repeat length × stroke height (RGBA).
W, H = 256, 48


def blank() -> Image.Image:
    return Image.new("RGBA", (W, H), (0, 0, 0, 0))


def ink(draw: ImageDraw.ImageDraw, pts: list[tuple[float, float]], color: tuple[int, int, int, int], width: float) -> None:
    if len(pts) < 2:
        return
    draw.line(pts, fill=color, width=max(1, int(round(width))), joint="curve")


def wave_y(x: float, amp: float = 4.0, phase: float = 0.0, period: float = 64.0) -> float:
    return H * 0.5 + amp * math.sin((x + phase) * (2.0 * math.pi / period))


def stroke_political_border() -> Image.Image:
    """Dashed oxblood political border — land-side political cue only."""
    im = blank()
    d = ImageDraw.Draw(im)
    color = (126, 69, 35, 235)
    shadow = (40, 24, 14, 90)
    x = 4.0
    dash, gap = 14.0, 10.0
    while x < W - 4:
        x1 = min(W - 4, x + dash)
        pts = [(t, wave_y(t, amp=3.5, period=72)) for t in [x + i * 2 for i in range(int((x1 - x) / 2) + 1)]]
        if len(pts) >= 2:
            ink(d, [(p[0], p[1] + 1) for p in pts], shadow, 4.0)
            ink(d, pts, color, 2.5)
        x = x1 + gap
    return im


def stroke_track() -> Image.Image:
    """Thin dashed wilderness track."""
    im = blank()
    d = ImageDraw.Draw(im)
    color = (100, 80, 55, 220)
    shadow = (30, 22, 14, 80)
    x = 2.0
    dash, gap = 8.0, 7.0
    while x < W - 2:
        x1 = min(W - 2, x + dash)
        pts = [(t, wave_y(t, amp=2.5, period=80)) for t in [x + i for i in range(int(x1 - x) + 1)]]
        if len(pts) >= 2:
            ink(d, [(p[0], p[1] + 1) for p in pts], shadow, 3.0)
            ink(d, pts, color, 1.5)
        x = x1 + gap
    return im


def stroke_road() -> Image.Image:
    """Solid single-stroke road."""
    im = blank()
    d = ImageDraw.Draw(im)
    color = (75, 58, 38, 230)
    shadow = (25, 18, 12, 90)
    pts = [(float(x), wave_y(float(x), amp=3.0, period=96)) for x in range(0, W, 2)]
    ink(d, [(p[0], p[1] + 1.5) for p in pts], shadow, 4.5)
    ink(d, pts, color, 2.5)
    return im


def stroke_highway() -> Image.Image:
    """Double-line highway with mile-post dots."""
    im = blank()
    d = ImageDraw.Draw(im)
    color = (55, 42, 28, 235)
    shadow = (20, 14, 10, 95)
    pts = [(float(x), wave_y(float(x), amp=2.2, period=110)) for x in range(0, W, 2)]
    for off in (-3.0, 3.0):
        shifted = [(p[0], p[1] + off) for p in pts]
        ink(d, [(p[0], p[1] + 1) for p in shifted], shadow, 3.5)
        ink(d, shifted, color, 2.0)
    # Mile markers along centerline.
    x = 18.0
    while x < W - 8:
        cy = wave_y(x, amp=2.2, period=110)
        r = 2.4
        d.ellipse((x - r, cy - r, x + r, cy + r), fill=color)
        x += 28.0
    return im


def stroke_ferry() -> Image.Image:
    """Water-dashed ferry route."""
    im = blank()
    d = ImageDraw.Draw(im)
    color = (90, 130, 145, 230)
    shadow = (30, 45, 55, 80)
    x = 3.0
    dash, gap = 10.0, 8.0
    while x < W - 3:
        x1 = min(W - 3, x + dash)
        pts = [(t, wave_y(t, amp=4.0, period=70, phase=12)) for t in [x + i for i in range(int(x1 - x) + 1)]]
        if len(pts) >= 2:
            ink(d, [(p[0], p[1] + 1) for p in pts], shadow, 3.5)
            ink(d, pts, color, 2.0)
        x = x1 + gap
    return im


def stroke_river() -> Image.Image:
    """Blue-gray continuous river ink (for authored river polylines)."""
    im = blank()
    d = ImageDraw.Draw(im)
    outer = (70, 110, 125, 200)
    inner = (140, 175, 185, 220)
    shadow = (25, 40, 50, 70)
    pts = [(float(x), wave_y(float(x), amp=5.0, period=88, phase=20)) for x in range(0, W, 2)]
    ink(d, [(p[0], p[1] + 2) for p in pts], shadow, 7.0)
    ink(d, pts, outer, 5.0)
    ink(d, pts, inner, 2.0)
    return im


STROKES = {
    "stroke-political-border.png": stroke_political_border,
    "stroke-track.png": stroke_track,
    "stroke-road.png": stroke_road,
    "stroke-highway.png": stroke_highway,
    "stroke-ferry.png": stroke_ferry,
    "stroke-river.png": stroke_river,
}


def write_provenance() -> None:
    (CONTEXT / "PROVENANCE.md").write_text(
        "# Cartography stroke tiles provenance\n\n"
        "- Generated 2026-07-20 by `tools/generate_cartography_strokes.py` (project-owned procedural ink).\n"
        "- Transparent RGBA tiles (256×48) for Map Canvas image-stamp rendering along authored XZ polylines.\n"
        "- Styles: political border, track, road, highway, ferry, river.\n"
        "- Mountains are **not** stroke overlays — they remain in discrete world-map plates.\n"
        "- Runtime copies: `samples/open-world-rpg/assets/ui/cartography/strokes/`.\n"
        "- Coast / ridge natural edges stay in plate art; political strokes sit on the land side only.\n",
        encoding="utf-8",
    )


def validate_rgba(path: Path) -> None:
    im = Image.open(path)
    if im.mode != "RGBA":
        raise SystemExit(f"{path} must be RGBA, got {im.mode}")
    if im.size != (W, H):
        raise SystemExit(f"{path} size {im.size} != {(W, H)}")
    # Must have at least some transparent and some opaque pixels.
    alphas = [px[3] for px in im.getdata()]
    if min(alphas) > 0:
        raise SystemExit(f"{path} has no transparent pixels")
    if max(alphas) < 200:
        raise SystemExit(f"{path} has no opaque ink")


def main() -> None:
    CONTEXT.mkdir(parents=True, exist_ok=True)
    RUNTIME.mkdir(parents=True, exist_ok=True)
    for name, factory in STROKES.items():
        im = factory()
        for dest_dir in (CONTEXT, RUNTIME):
            path = dest_dir / name
            im.save(path)
            validate_rgba(path)
            print(f"wrote {path} ({im.size}, RGBA)")
    write_provenance()
    # Also keep a copy of provenance next to runtime for packagers.
    (RUNTIME / "PROVENANCE.md").write_text((CONTEXT / "PROVENANCE.md").read_text(encoding="utf-8"), encoding="utf-8")
    print("done")


if __name__ == "__main__":
    main()
