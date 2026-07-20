#!/usr/bin/env python3
"""Build discrete Cartography zoom-layer plates from the continuous world-map master.

Crops named theaters from the master (no AI quadrant inject) and writes a manifest
used by Map Canvas Cartography + future player map (TICKET-0061).
"""
from __future__ import annotations

import json
import shutil
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
MASTER = ROOT / "context" / "art" / "cartography" / "world-map-master.png"
CONTEXT_OUT = ROOT / "context" / "art" / "cartography" / "world-map-layers"
RUNTIME_OUT = ROOT / "samples" / "open-world-rpg" / "assets" / "ui" / "cartography" / "world-map-layers"

# Normalized UV crops on the master (u right, v down). Overlaps intentional for soft thresholds.
LAYERS = (
    {
        "id": "continent",
        "file": "continent.png",
        "u0": 0.0,
        "v0": 0.0,
        "u1": 1.0,
        "v1": 1.0,
        "minZoom": 0.0,
        "priority": 0,
        "role": "Full Tessera geography",
    },
    {
        "id": "theater_nw",
        "file": "theater_nw.png",
        "u0": 0.0,
        "v0": 0.0,
        "u1": 0.55,
        "v1": 0.55,
        "minZoom": 1.15,
        "priority": 1,
        "role": "NW continent / western spine",
    },
    {
        "id": "theater_ne",
        "file": "theater_ne.png",
        "u0": 0.45,
        "v0": 0.0,
        "u1": 1.0,
        "v1": 0.55,
        "minZoom": 1.15,
        "priority": 1,
        "role": "NE land + northeastern island",
    },
    {
        "id": "theater_sw",
        "file": "theater_sw.png",
        "u0": 0.0,
        "v0": 0.45,
        "u1": 0.55,
        "v1": 1.0,
        "minZoom": 1.15,
        "priority": 1,
        "role": "SW coasts / Act 0 approaches",
    },
    {
        "id": "theater_se",
        "file": "theater_se.png",
        "u0": 0.45,
        "v0": 0.45,
        "u1": 1.0,
        "v1": 1.0,
        "minZoom": 1.15,
        "priority": 1,
        "role": "SE / scorched coast",
    },
    {
        "id": "theater_interior_sea",
        "file": "theater_interior_sea.png",
        "u0": 0.25,
        "v0": 0.25,
        "u1": 0.75,
        "v1": 0.75,
        "minZoom": 1.35,
        "priority": 2,
        "role": "Interior sea + central island",
    },
    {
        "id": "local_calrenoth",
        "file": "local_calrenoth.png",
        "u0": 0.05,
        "v0": 0.55,
        "u1": 0.42,
        "v1": 0.92,
        "minZoom": 2.0,
        "priority": 3,
        "role": "Opening theater (SW cluster; refine when slice footprint locks)",
    },
)

PLATE_WIDTH = 2048


def crop_plate(master: Image.Image, layer: dict) -> Image.Image:
    mw, mh = master.size
    x0 = int(round(layer["u0"] * mw))
    y0 = int(round(layer["v0"] * mh))
    x1 = int(round(layer["u1"] * mw))
    y1 = int(round(layer["v1"] * mh))
    x0 = max(0, min(x0, mw - 1))
    y0 = max(0, min(y0, mh - 1))
    x1 = max(x0 + 1, min(x1, mw))
    y1 = max(y0 + 1, min(y1, mh))
    crop = master.crop((x0, y0, x1, y1))
    tw = PLATE_WIDTH
    th = max(1, int(round(tw * crop.height / crop.width)))
    return crop.resize((tw, th), Image.Resampling.LANCZOS)


def main() -> None:
    if not MASTER.exists():
        raise SystemExit(f"missing master: {MASTER}")
    master = Image.open(MASTER).convert("RGB")
    print(f"master {MASTER.name} {master.size}")

    for out in (CONTEXT_OUT, RUNTIME_OUT):
        if out.exists():
            shutil.rmtree(out)
        out.mkdir(parents=True, exist_ok=True)

    layers_out = []
    for layer in LAYERS:
        plate = crop_plate(master, layer)
        for root in (CONTEXT_OUT, RUNTIME_OUT):
            path = root / layer["file"]
            plate.save(path, optimize=True)
        print(f"  wrote {layer['id']} {plate.size}")
        layers_out.append(
            {
                "id": layer["id"],
                "file": layer["file"],
                "u0": layer["u0"],
                "v0": layer["v0"],
                "u1": layer["u1"],
                "v1": layer["v1"],
                "minZoom": layer["minZoom"],
                "priority": layer["priority"],
                "width": plate.width,
                "height": plate.height,
                "role": layer["role"],
            }
        )

    manifest = {
        "schemaVersion": 1,
        "id": "official_world_map_layers",
        "masterWidth": master.width,
        "masterHeight": master.height,
        "aspect": master.width / master.height,
        "nativeWidth": master.width,
        "transitionSeconds": 0.35,
        "notes": (
            "Discrete Cartography zoom plates cropped from continuous master. "
            "Fog transition between plates; ornate frame is separate chrome."
        ),
        "layers": layers_out,
    }
    text = json.dumps(manifest, indent=2) + "\n"
    for root in (CONTEXT_OUT, RUNTIME_OUT):
        (root / "manifest.json").write_text(text, encoding="utf-8")
        print(f"wrote {root / 'manifest.json'}")
    print(f"done ({len(layers_out)} layers)")


if __name__ == "__main__":
    main()
