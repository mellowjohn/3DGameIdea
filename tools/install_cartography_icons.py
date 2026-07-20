#!/usr/bin/env python3
"""Install regenerated cartography icons with transparent backgrounds + tight crops."""
from __future__ import annotations

import shutil
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
GEN = Path(r"C:\Users\johnr\.cursor\projects\c-Users-johnr-Documents-3DGameIdea\assets")
CONTEXT = ROOT / "context" / "art" / "cartography"
RUNTIME = ROOT / "samples" / "open-world-rpg" / "assets" / "ui" / "cartography"

# Generated name -> installed runtime name
ICON_MAP = {
    "icon-village.png": "icon-village.png",
    "icon-town.png": "icon-town.png",
    "icon-city.png": "icon-city.png",
    "icon-fortress.png": "icon-fortress.png",
    "icon-ruin.png": "icon-ruin.png",
    "icon-gate.png": "icon-gate.png",
    "icon-shrine.png": "icon-shrine.png",
    "icon-camp.png": "icon-camp.png",
    "icon-landmark.png": "icon-landmark.png",
    "icon-dock.png": "icon-dock.png",
    "heraldry-tessera.png": "heraldry-kingdom_tessera.png",
    "heraldry-imperium.png": "heraldry-chaotic_imperium.png",
    "heraldry-cristallo.png": "heraldry-cristallo.png",
    "heraldry-arrotrebae.png": "heraldry-arrotrebae.png",
    "heraldry-orc.png": "heraldry-orc_warbands.png",
    "heraldry-thalassar.png": "heraldry-thalassar.png",
    "heraldry-underflow.png": "heraldry-underflow.png",
}


def corner_bg_color(im: Image.Image) -> tuple[int, int, int]:
    px = im.load()
    w, h = im.size
    samples = [
        px[2, 2][:3],
        px[w - 3, 2][:3],
        px[2, h - 3][:3],
        px[w - 3, h - 3][:3],
        px[w // 2, 2][:3],
        px[2, h // 2][:3],
    ]
    r = sum(c[0] for c in samples) // len(samples)
    g = sum(c[1] for c in samples) // len(samples)
    b = sum(c[2] for c in samples) // len(samples)
    return r, g, b


def make_transparent(im: Image.Image, soft: float = 28.0, hard: float = 52.0) -> Image.Image:
    """Knock out near-corner background (light gray / parchment) into alpha."""
    im = im.convert("RGBA")
    bg = corner_bg_color(im)
    px = im.load()
    w, h = im.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            dist = ((r - bg[0]) ** 2 + (g - bg[1]) ** 2 + (b - bg[2]) ** 2) ** 0.5
            # Also treat near-white / near-neutral light fills as background.
            luma = 0.2126 * r + 0.7152 * g + 0.0722 * b
            chroma = max(r, g, b) - min(r, g, b)
            light_bg = luma > 210 and chroma < 18
            score = dist
            if light_bg:
                score = min(score, soft * 0.35)
            if score <= soft:
                px[x, y] = (r, g, b, 0)
            elif score < hard:
                t = (score - soft) / (hard - soft)
                px[x, y] = (r, g, b, max(0, min(255, int(a * t))))
    return im


def crop_to_alpha(im: Image.Image, pad: int = 8) -> Image.Image:
    bbox = im.getbbox()
    if not bbox:
        return im
    l, t, r, b = bbox
    l = max(0, l - pad)
    t = max(0, t - pad)
    r = min(im.width, r + pad)
    b = min(im.height, b + pad)
    return im.crop((l, t, r, b))


def fit_square(im: Image.Image, size: int = 128) -> Image.Image:
    im = crop_to_alpha(im)
    w, h = im.size
    side = max(w, h)
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    canvas.paste(im, ((side - w) // 2, (side - h) // 2), im)
    return canvas.resize((size, size), Image.Resampling.LANCZOS)


def process_one(src: Path) -> Image.Image:
    return fit_square(make_transparent(Image.open(src)))


def write_provenance() -> None:
    (CONTEXT / "PROVENANCE.md").write_text(
        "# Cartography art provenance\n\n"
        "- Regenerated 2026-07-20 via Cursor GenerateImage as isolated icons/heraldry.\n"
        "- Post-processed by `tools/install_cartography_icons.py` for transparent backgrounds "
        "and tight square crops (128×128).\n"
        "- Project-owned AI art. Runtime copies: `samples/open-world-rpg/assets/ui/cartography/`.\n"
        "- Cartography Map Canvas backdrop uses `context/story/official-world-map.png`.\n"
        "- No invented place names on artwork.\n",
        encoding="utf-8",
    )


def main() -> None:
    CONTEXT.mkdir(parents=True, exist_ok=True)
    RUNTIME.mkdir(parents=True, exist_ok=True)
    for src_name, dest_name in ICON_MAP.items():
        src = GEN / src_name
        if not src.exists():
            raise SystemExit(f"missing generated asset: {src}")
        out = process_one(src)
        for dest_dir in (CONTEXT, RUNTIME):
            path = dest_dir / dest_name
            out.save(path)
            print(f"wrote {path} ({out.size}, alpha)")
    write_provenance()
    print("done")


if __name__ == "__main__":
    main()
