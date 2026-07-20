#!/usr/bin/env python3
"""Generate aged parchment fill + ornate map-label border for Cartography panels."""

from __future__ import annotations

import math
import os
import random
import shutil
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
SRC = Path(
    r"C:\Users\johnr\.cursor\projects\c-Users-johnr-Documents-3DGameIdea\assets\panel-parchment.png"
)
CTX = ROOT / "context" / "art" / "cartography" / "panel"
SAMPLE = ROOT / "samples" / "open-world-rpg" / "assets" / "ui" / "cartography" / "panel"


def process_parchment(src: Path) -> Image.Image:
    img = Image.open(src).convert("RGBA")
    w, h = img.size
    r, g, b, a = img.split()
    r = ImageEnhance.Brightness(r).enhance(1.02)
    b = ImageEnhance.Brightness(b).enhance(0.96)
    img = Image.merge("RGBA", (r, g, b, a))

    overlay = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    for i in range(28):
        alpha = int(18 * (1 - i / 28))
        od.rectangle([i, i, w - 1 - i, h - 1 - i], outline=(90, 70, 45, alpha))
    img = Image.alpha_composite(img, overlay)

    rng = random.Random(42)
    noise = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    nd = ImageDraw.Draw(noise)
    for _ in range(900):
        x, y = rng.randrange(w), rng.randrange(h)
        c = rng.choice([(160, 130, 90, 18), (120, 95, 70, 14), (200, 175, 140, 12)])
        nd.point((x, y), fill=c)
    return Image.alpha_composite(img, noise)


def ink(a: int) -> tuple[int, int, int, int]:
    return (58, 42, 28, a)


def make_border(bw: int = 512, bh: int = 384) -> Image.Image:
    border = Image.new("RGBA", (bw, bh), (0, 0, 0, 0))
    d = ImageDraw.Draw(border)

    for t in range(6):
        d.rectangle([4 + t, 4 + t, bw - 5 - t, bh - 5 - t], outline=(140, 115, 80, 40 - t * 4))

    for t in range(3):
        d.rectangle([14 + t, 14 + t, bw - 15 - t, bh - 15 - t], outline=ink(210 - t * 30))
    for t in range(2):
        d.rectangle([22 + t, 22 + t, bw - 23 - t, bh - 23 - t], outline=ink(160 - t * 20))
    d.rectangle([28, 28, bw - 29, bh - 29], outline=ink(120))

    def corner(cx: float, cy: float, sx: int, sy: int) -> None:
        pts = []
        for i in range(0, 36):
            ang = math.radians(i * 2.4)
            radius = 18 + 6 * math.sin(ang * 2)
            pts.append((cx + sx * (8 + radius * math.cos(ang)), cy + sy * (8 + radius * math.sin(ang))))
        if len(pts) > 1:
            d.line(pts, fill=ink(200), width=2)
        d.polygon(
            [(cx, cy - 10 * sy), (cx + 8 * sx, cy), (cx, cy + 10 * sy), (cx - 8 * sx, cy)],
            outline=ink(220),
        )
        d.ellipse([cx - 3, cy - 3, cx + 3, cy + 3], outline=ink(200))
        for off in (16, 28, 40):
            d.line([(cx + sx * off, cy), (cx + sx * (off + 8), cy - sy * 6)], fill=ink(150), width=1)
            d.line([(cx, cy + sy * off), (cx - sx * 6, cy + sy * (off + 8))], fill=ink(150), width=1)

    corner(42, 42, 1, 1)
    corner(bw - 43, 42, -1, 1)
    corner(42, bh - 43, 1, -1)
    corner(bw - 43, bh - 43, -1, -1)

    for x in range(70, bw - 70, 28):
        d.ellipse([x - 2, 17, x + 2, 21], fill=ink(140))
        d.ellipse([x - 2, bh - 22, x + 2, bh - 18], fill=ink(140))
    for y in range(70, bh - 70, 28):
        d.ellipse([17, y - 2, 21, y + 2], fill=ink(140))
        d.ellipse([bw - 22, y - 2, bw - 18, y + 2], fill=ink(140))

    ink_layer = border.filter(ImageFilter.GaussianBlur(0.6))
    return Image.alpha_composite(ink_layer, border)


def main() -> None:
    CTX.mkdir(parents=True, exist_ok=True)
    SAMPLE.mkdir(parents=True, exist_ok=True)

    if not SRC.exists():
        raise SystemExit(f"missing source parchment: {SRC}")

    parchment = process_parchment(SRC)
    parch_path = CTX / "panel-parchment.png"
    parchment.save(parch_path, "PNG")
    shutil.copy2(parch_path, SAMPLE / "panel-parchment.png")

    border = make_border()
    border_path = CTX / "panel-border.png"
    border.save(border_path, "PNG")
    shutil.copy2(border_path, SAMPLE / "panel-border.png")

    wide = make_border(720, 160)
    wide_path = CTX / "panel-border-wide.png"
    wide.save(wide_path, "PNG")
    shutil.copy2(wide_path, SAMPLE / "panel-border-wide.png")

    print(f"wrote {parch_path} {parchment.size}")
    print(f"wrote {border_path} {border.size}")
    print(f"wrote {wide_path} {wide.size}")


if __name__ == "__main__":
    main()
