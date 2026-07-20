#!/usr/bin/env python3
"""Generate fantasy cartography icon sheets, heraldry, and stroke references."""
from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "context" / "art" / "cartography"
RUNTIME = ROOT / "samples" / "open-world-rpg" / "assets" / "ui" / "cartography"

INK = (42, 36, 32, 255)
PARCHMENT = (201, 184, 150, 255)
SEA = (106, 122, 130, 255)
GOLD = (140, 110, 55, 255)
CRIMSON = (120, 35, 40, 255)
FOREST = (70, 90, 55, 255)
CRYSTAL = (90, 120, 140, 255)
ASH = (80, 70, 65, 255)


def ensure_dirs() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    RUNTIME.mkdir(parents=True, exist_ok=True)


def save(img: Image.Image, name: str) -> None:
    path = OUT / name
    img.save(path)
    img.save(RUNTIME / name)
    print(f"wrote {path}")


def cell(draw: ImageDraw.ImageDraw, x0: int, y0: int, size: int, label: str) -> None:
    draw.rectangle([x0, y0, x0 + size - 1, y0 + size - 1], outline=(160, 140, 110, 255), width=1)


def draw_village(d: ImageDraw.ImageDraw, cx: int, cy: int, s: float = 1.0) -> None:
    for dx, dy in ((-14, 4), (0, 8), (14, 2)):
        x, y = int(cx + dx * s), int(cy + dy * s)
        d.polygon([(x - 8 * s, y), (x, y - 10 * s), (x + 8 * s, y), (x + 8 * s, y + 8 * s), (x - 8 * s, y + 8 * s)], outline=INK)


def draw_town(d: ImageDraw.ImageDraw, cx: int, cy: int, s: float = 1.0) -> None:
    draw_village(d, cx, cy + 6, s * 0.85)
    d.rectangle([cx - 6 * s, cy - 18 * s, cx + 6 * s, cy - 2 * s], outline=INK, width=2)
    d.polygon([(cx - 10 * s, cy - 18 * s), (cx, cy - 28 * s), (cx + 10 * s, cy - 18 * s)], outline=INK)


def draw_city(d: ImageDraw.ImageDraw, cx: int, cy: int, s: float = 1.0) -> None:
    d.ellipse([cx - 28 * s, cy - 18 * s, cx + 28 * s, cy + 22 * s], outline=INK, width=2)
    for dx in (-16, 0, 16):
        d.rectangle([cx + dx * s - 4 * s, cy - 26 * s, cx + dx * s + 4 * s, cy - 6 * s], outline=INK, width=2)
        d.polygon(
            [
                (cx + dx * s - 6 * s, cy - 26 * s),
                (cx + dx * s, cy - 34 * s),
                (cx + dx * s + 6 * s, cy - 26 * s),
            ],
            outline=INK,
        )


def draw_fortress(d: ImageDraw.ImageDraw, cx: int, cy: int, s: float = 1.0) -> None:
    d.rectangle([cx - 22 * s, cy - 8 * s, cx + 22 * s, cy + 18 * s], outline=INK, width=2)
    for i in range(-2, 3):
        x = cx + i * 10 * s
        d.rectangle([x - 3 * s, cy - 18 * s, x + 3 * s, cy - 8 * s], outline=INK, width=2)
    d.rectangle([cx - 6 * s, cy + 4 * s, cx + 6 * s, cy + 18 * s], outline=INK, width=2)


def draw_ruin(d: ImageDraw.ImageDraw, cx: int, cy: int, s: float = 1.0) -> None:
    d.line([(cx - 18 * s, cy + 14 * s), (cx - 10 * s, cy - 16 * s), (cx + 2 * s, cy - 4 * s)], fill=INK, width=2)
    d.line([(cx + 4 * s, cy - 2 * s), (cx + 16 * s, cy + 16 * s)], fill=INK, width=2)
    d.line([(cx - 20 * s, cy + 16 * s), (cx + 20 * s, cy + 16 * s)], fill=INK, width=1)


def make_settlements() -> None:
    size, cols = 128, 5
    img = Image.new("RGBA", (size * cols, size), PARCHMENT)
    d = ImageDraw.Draw(img)
    drawers = [draw_village, draw_town, draw_city, draw_fortress, draw_ruin]
    labels = ["village", "town", "city", "fortress", "ruin"]
    for i, (fn, lab) in enumerate(zip(drawers, labels)):
        x0 = i * size
        cell(d, x0, 0, size, lab)
        fn(d, x0 + size // 2, size // 2 + 4)
        d.text((x0 + 8, size - 16), lab, fill=INK)
    save(img, "icon-sheet-settlements.png")


def draw_gate(d: ImageDraw.ImageDraw, cx: int, cy: int) -> None:
    d.arc([cx - 20, cy - 24, cx + 20, cy + 16], 180, 0, fill=INK, width=2)
    d.line([(cx - 20, cy + 16), (cx - 20, cy - 4)], fill=INK, width=2)
    d.line([(cx + 20, cy + 16), (cx + 20, cy - 4)], fill=INK, width=2)
    d.line([(cx - 20, cy + 16), (cx + 20, cy + 16)], fill=INK, width=2)


def draw_shrine(d: ImageDraw.ImageDraw, cx: int, cy: int) -> None:
    d.polygon([(cx, cy - 28), (cx + 14, cy - 4), (cx, cy + 8), (cx - 14, cy - 4)], outline=INK)
    d.line([(cx, cy + 8), (cx, cy + 22)], fill=INK, width=2)
    d.ellipse([cx - 10, cy + 18, cx + 10, cy + 26], outline=INK)


def draw_camp(d: ImageDraw.ImageDraw, cx: int, cy: int) -> None:
    d.polygon([(cx - 22, cy + 12), (cx, cy - 22), (cx + 22, cy + 12)], outline=INK, width=2)
    d.ellipse([cx - 6, cy + 14, cx + 6, cy + 24], outline=CRIMSON)
    d.line([(cx, cy + 14), (cx, cy + 8)], fill=CRIMSON, width=1)


def draw_landmark(d: ImageDraw.ImageDraw, cx: int, cy: int) -> None:
    d.rectangle([cx - 4, cy - 24, cx + 4, cy + 16], outline=INK, width=2)
    d.ellipse([cx - 14, cy - 34, cx + 14, cy - 10], outline=INK, width=2)
    d.line([(cx - 18, cy + 16), (cx + 18, cy + 16)], fill=INK, width=2)


def draw_dock(d: ImageDraw.ImageDraw, cx: int, cy: int) -> None:
    d.rectangle([cx - 24, cy - 4, cx + 8, cy + 8], outline=INK, width=2)
    d.line([(cx - 18, cy + 8), (cx - 18, cy + 20)], fill=INK, width=2)
    d.line([(cx - 4, cy + 8), (cx - 4, cy + 20)], fill=INK, width=2)
    d.ellipse([cx + 10, cy - 10, cx + 28, cy + 8], outline=SEA, width=2)
    d.line([(cx + 19, cy - 2), (cx + 19, cy + 14)], fill=INK, width=2)
    d.line([(cx + 19, cy + 4), (cx + 28, cy + 4)], fill=INK, width=2)


def make_landmarks() -> None:
    size, cols = 128, 5
    img = Image.new("RGBA", (size * cols, size), PARCHMENT)
    d = ImageDraw.Draw(img)
    drawers = [draw_gate, draw_shrine, draw_camp, draw_landmark, draw_dock]
    labels = ["gate", "shrine", "camp", "landmark", "dock"]
    for i, (fn, lab) in enumerate(zip(drawers, labels)):
        x0 = i * size
        cell(d, x0, 0, size, lab)
        fn(d, x0 + size // 2, size // 2)
        d.text((x0 + 8, size - 16), lab, fill=INK)
    save(img, "icon-sheet-landmarks.png")


def shield(d: ImageDraw.ImageDraw, cx: int, cy: int, fill) -> None:
    pts = [(cx, cy - 36), (cx + 28, cy - 24), (cx + 24, cy + 10), (cx, cy + 36), (cx - 24, cy + 10), (cx - 28, cy - 24)]
    d.polygon(pts, fill=fill, outline=INK)


def make_heraldry() -> None:
    specs = [
        ("heraldry-kingdom_tessera.png", GOLD, "tree"),
        ("heraldry-chaotic_imperium.png", CRIMSON, "hollow"),
        ("heraldry-cristallo.png", CRYSTAL, "crystal"),
        ("heraldry-arrotrebae.png", FOREST, "antler"),
        ("heraldry-orc_warbands.png", ASH, "tusk"),
    ]
    for name, fill, motif in specs:
        img = Image.new("RGBA", (128, 128), (0, 0, 0, 0))
        d = ImageDraw.Draw(img)
        shield(d, 64, 64, fill)
        if motif == "tree":
            d.line([(64, 88), (64, 40)], fill=INK, width=3)
            d.ellipse([48, 28, 80, 58], outline=INK, width=2)
        elif motif == "hollow":
            d.ellipse([48, 40, 80, 72], outline=INK, width=3)
            d.line([(54, 54), (74, 54)], fill=INK, width=2)
        elif motif == "crystal":
            d.polygon([(64, 28), (82, 64), (64, 90), (46, 64)], outline=INK, width=2)
        elif motif == "antler":
            d.line([(64, 88), (64, 50)], fill=INK, width=3)
            d.arc([40, 30, 64, 70], 200, 340, fill=INK, width=2)
            d.arc([64, 30, 88, 70], 200, 340, fill=INK, width=2)
        else:
            d.arc([40, 48, 64, 90], 270, 90, fill=INK, width=3)
            d.arc([64, 48, 88, 90], 90, 270, fill=INK, width=3)
        save(img, name)


def make_border_strokes() -> None:
    img = Image.new("RGBA", (512, 160), PARCHMENT)
    d = ImageDraw.Draw(img)
    d.text((12, 8), "natural coast / river", fill=INK)
    for x in range(20, 480, 2):
        y = 50 + int(8 * math.sin(x * 0.05))
        d.point((x, y), fill=INK)
        d.point((x, y + 1), fill=INK)
    d.line([(20, 50), (480, 50)], fill=SEA, width=1)
    d.text((12, 78), "political (dashed faction tint)", fill=INK)
    x = 20
    while x < 480:
        d.line([(x, 110), (x + 14, 110)], fill=CRIMSON, width=2)
        x += 22
    save(img, "border-strokes-reference.png")


def make_travel_strokes() -> None:
    img = Image.new("RGBA", (512, 220), PARCHMENT)
    d = ImageDraw.Draw(img)
    rows = [
        (40, "track", "dash"),
        (90, "road", "solid"),
        (140, "highway", "double"),
        (190, "ferry", "water"),
    ]
    for y, label, style in rows:
        d.text((12, y - 18), label, fill=INK)
        if style == "dash":
            x = 100
            while x < 480:
                d.line([(x, y), (x + 10, y)], fill=INK, width=1)
                x += 16
        elif style == "solid":
            d.line([(100, y), (480, y)], fill=INK, width=2)
        elif style == "double":
            d.line([(100, y - 3), (480, y - 3)], fill=INK, width=2)
            d.line([(100, y + 3), (480, y + 3)], fill=INK, width=2)
            for x in range(140, 460, 60):
                d.ellipse([x - 2, y - 2, x + 2, y + 2], fill=INK)
        else:
            x = 100
            while x < 480:
                d.line([(x, y), (x + 12, y)], fill=SEA, width=2)
                x += 20
    save(img, "travel-strokes-reference.png")


def main() -> None:
    ensure_dirs()
    make_settlements()
    make_landmarks()
    make_heraldry()
    make_border_strokes()
    make_travel_strokes()
    (OUT / "PROVENANCE.md").write_text(
        "# Cartography art provenance\n\n"
        "- Generated 2026-07-20 for Tessera fantasy cartography kit.\n"
        "- Project-owned AI/procedural placeholders (Python PIL).\n"
        "- Icons and heraldry are draft-safe; no invented place names.\n"
        "- Runtime copies: `samples/open-world-rpg/assets/ui/cartography/`.\n",
        encoding="utf-8",
    )
    print("done")


if __name__ == "__main__":
    main()
