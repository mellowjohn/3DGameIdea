#!/usr/bin/env python3
"""Build multi-LOD world-map tiles for Cartography Map Canvas backdrop.

Builds a higher-res continuous master from the clean overview. Optional
`--with-detail-plates` feather-injects NW/NE/SW/SE AI plates (off by default —
independent plates do not share geography and bake visible 2x2 seams).
"""
from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
GEN = Path(r"C:\Users\johnr\.cursor\projects\c-Users-johnr-Documents-3DGameIdea\assets")
STORY = ROOT / "context" / "story"
CONTEXT_TILES = ROOT / "context" / "art" / "cartography" / "world-map-tiles"
RUNTIME_TILES = ROOT / "samples" / "open-world-rpg" / "assets" / "ui" / "cartography" / "world-map-tiles"

# Prefer newer hires plate when present.
MASTER_CANDIDATES = (
    "official-world-map-clean-hires.png",
    "official-world-map-clean.png",
)
DETAIL_QUADS = {
    "nw": "world-map-detail-nw.png",
    "ne": "world-map-detail-ne.png",
    "sw": "world-map-detail-sw.png",
    "se": "world-map-detail-se.png",
}

TILE_SIZE = 512
TARGET_W = 4096
OCEAN = (74, 98, 108)
# Center of each quarter can take this much detail plate (edges stay overview).
DETAIL_BLEND = 0.55
FEATHER_MARGIN = 0.22  # fraction of quarter size faded to overview-only


def load_rgb(path: Path) -> Image.Image:
    return Image.open(path).convert("RGB")


def feather_mask(size: tuple[int, int], margin_frac: float) -> Image.Image:
    w, h = size
    mask = Image.new("L", (w, h), 0)
    draw = ImageDraw.Draw(mask)
    mx = int(w * margin_frac)
    my = int(h * margin_frac)
    # Soft stepped bands toward full opacity in the center.
    bands = 12
    for i in range(bands):
        t = (i + 1) / bands
        inset_x = int(mx * (1.0 - t))
        inset_y = int(my * (1.0 - t))
        alpha = int(255 * t)
        draw.rectangle((inset_x, inset_y, w - 1 - inset_x, h - 1 - inset_y), fill=alpha)
    return mask.filter(ImageFilter.GaussianBlur(radius=max(8, min(w, h) // 40)))


def build_master(*, with_detail_plates: bool) -> tuple[Image.Image, int, int]:
    base_path = None
    for name in MASTER_CANDIDATES:
        candidate = GEN / name
        if candidate.exists():
            base_path = candidate
            break
    if base_path is None:
        base_path = STORY / "official-world-map.png"
    if not base_path.exists():
        raise SystemExit("missing clean world-map master")

    base = load_rgb(base_path)
    native_w, native_h = base.size
    target_h = int(round(TARGET_W * base.height / base.width))
    target_h -= target_h % 2
    master = base.resize((TARGET_W, target_h), Image.Resampling.LANCZOS)
    master = master.filter(ImageFilter.UnsharpMask(radius=1.2, percent=110, threshold=2))
    print(f"base {base_path.name} {base.size} -> master {master.size}")

    if with_detail_plates:
        quads = {}
        for key, name in DETAIL_QUADS.items():
            path = GEN / name
            if path.exists():
                quads[key] = load_rgb(path)
        if len(quads) != 4:
            print(f"detail plates: {len(quads)}/4 — skipping feathered inject")
        else:
            mw, mh = master.size
            half_w, half_h = mw // 2, mh // 2
            boxes = {
                "nw": (0, 0, half_w, half_h),
                "ne": (half_w, 0, mw, half_h),
                "sw": (0, half_h, half_w, mh),
                "se": (half_w, half_h, mw, mh),
            }
            for key, box in boxes.items():
                crop = master.crop(box)
                detail = quads[key].resize(crop.size, Image.Resampling.LANCZOS)
                # Keep overview geography; pull high-frequency paint from the detail plate.
                mixed = Image.blend(crop, detail, DETAIL_BLEND)
                mask = feather_mask(crop.size, FEATHER_MARGIN)
                master.paste(mixed, box[:2], mask)
                print(f"  feathered detail {key} into {box}")
    else:
        print("detail plates: skipped (continuous overview only)")

    # Copy working clean source forward for provenance.
    clean_out = GEN / "official-world-map-clean.png"
    base.resize((1536, 1024), Image.Resampling.LANCZOS).save(clean_out)
    return master, master.width, master.height


def write_overview(master: Image.Image) -> None:
    overview = master.resize((1536, 1024), Image.Resampling.LANCZOS)
    dest = STORY / "official-world-map.png"
    overview.save(dest, optimize=True)
    print(f"wrote overview {dest} {overview.size}")
    CONTEXT_TILES.parent.mkdir(parents=True, exist_ok=True)
    master_path = CONTEXT_TILES.parent / "world-map-master.png"
    master.save(master_path, optimize=True)
    print(f"wrote master {master_path} {master.size}")


def emit_level(master: Image.Image, lod: int, level_w: int, out_dirs: list[Path]) -> dict:
    content_h = max(1, int(round(level_w * master.height / master.width)))
    rows = (content_h + TILE_SIZE - 1) // TILE_SIZE
    cols = (level_w + TILE_SIZE - 1) // TILE_SIZE
    level_h = rows * TILE_SIZE
    level_w_pad = cols * TILE_SIZE
    canvas = Image.new("RGB", (level_w_pad, level_h), OCEAN)
    fitted = (
        master
        if (level_w, content_h) == master.size
        else master.resize((level_w, content_h), Image.Resampling.LANCZOS)
    )
    canvas.paste(fitted, (0, 0))

    for y in range(rows):
        for x in range(cols):
            tile = canvas.crop((x * TILE_SIZE, y * TILE_SIZE, (x + 1) * TILE_SIZE, (y + 1) * TILE_SIZE))
            rel = Path(f"z{lod}") / f"{x}_{y}.png"
            for root in out_dirs:
                path = root / rel
                path.parent.mkdir(parents=True, exist_ok=True)
                tile.save(path, optimize=True)
            print(f"  wrote {rel}")

    return {
        "lod": lod,
        "cols": cols,
        "rows": rows,
        "contentWidth": level_w,
        "contentHeight": content_h,
        "levelWidth": level_w_pad,
        "levelHeight": level_h,
    }


def write_manifest(master: Image.Image, native_w: int, native_h: int, levels: list[dict], out_dirs: list[Path]) -> None:
    manifest = {
        "schemaVersion": 1,
        "id": "official_world_map_tiles",
        "tileSize": TILE_SIZE,
        "maxLod": levels[-1]["lod"] if levels else 0,
        "masterWidth": master.width,
        "masterHeight": master.height,
        "nativeWidth": native_w,
        "nativeHeight": native_h,
        "aspect": master.width / master.height,
        "pathPattern": "z{lod}/{x}_{y}.png",
        "levels": levels,
        "notes": (
            "4096-wide continuous master from hires overview (detail-plate inject off by default). "
            "Zoom cap uses nativeWidth (~1.35x allowed in editor)."
        ),
    }
    text = json.dumps(manifest, indent=2) + "\n"
    for root in out_dirs:
        (root / "manifest.json").write_text(text, encoding="utf-8")
        print(f"wrote {root / 'manifest.json'}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--with-detail-plates",
        action="store_true",
        help="Feather-inject NW/NE/SW/SE AI plates (usually creates 2x2 geography seams).",
    )
    args = parser.parse_args()

    if RUNTIME_TILES.exists():
        shutil.rmtree(RUNTIME_TILES)
    RUNTIME_TILES.mkdir(parents=True, exist_ok=True)
    CONTEXT_TILES.mkdir(parents=True, exist_ok=True)
    for stale in CONTEXT_TILES.glob("z*"):
        if stale.is_dir():
            shutil.rmtree(stale)

    master, native_w, native_h = build_master(with_detail_plates=args.with_detail_plates)
    write_overview(master)

    widths = [w for w in (512, 1024, 2048) if w < master.width]
    widths.append(master.width)

    out_dirs = [CONTEXT_TILES, RUNTIME_TILES]
    levels = []
    print(f"building {len(widths)} LODs @ {TILE_SIZE}px from {master.size}")
    for lod, level_w in enumerate(widths):
        levels.append(emit_level(master, lod, level_w, out_dirs))
    write_manifest(master, native_w, native_h, levels, out_dirs)
    total = sum(level["cols"] * level["rows"] for level in levels)
    print(f"done ({total} tiles -> {RUNTIME_TILES})")


if __name__ == "__main__":
    main()
