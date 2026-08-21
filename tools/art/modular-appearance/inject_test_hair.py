#!/usr/bin/env python3
"""Seed a Head-weighted spike hair mesh onto GoodPlayerModelCopy (slot test).

Not the live polish path — Blockbench MCP is preferred for hero hair. This
records a graybox hair-cap + spikes so the appearance socket can bind like
iron-test armor (keepMeshes + matchPlayerBake).
"""
from __future__ import annotations

import base64
import io
import json
import secrets
import uuid
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[3]
SRC = REPO / "tools/art/modular-iron-test/GoodPlayerModelCopy.bbmodel"
OUT = REPO / "tools/art/modular-appearance/TestHairSpikes.bbmodel"
MESH_NAME = "kit_test_hair_spikes"


def vkey() -> str:
    alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
    return "".join(secrets.choice(alphabet) for _ in range(4))


def add_box(vertices: dict[str, list[float]], faces: dict[str, dict],
            cx: float, cy: float, cz: float, sx: float, sy: float, sz: float) -> list[str]:
    hx, hy, hz = sx * 0.5, sy * 0.5, sz * 0.5
    keys = [vkey() for _ in range(8)]
    corners = [
        [cx - hx, cy - hy, cz - hz],
        [cx + hx, cy - hy, cz - hz],
        [cx + hx, cy + hy, cz - hz],
        [cx - hx, cy + hy, cz - hz],
        [cx - hx, cy - hy, cz + hz],
        [cx + hx, cy - hy, cz + hz],
        [cx + hx, cy + hy, cz + hz],
        [cx - hx, cy + hy, cz + hz],
    ]
    for key, pos in zip(keys, corners):
        vertices[key] = pos
    quads = [
        (0, 1, 2, 3),
        (5, 4, 7, 6),
        (4, 0, 3, 7),
        (1, 5, 6, 2),
        (3, 2, 6, 7),
        (4, 5, 1, 0),
    ]
    for a, b, c, d in quads:
        ids = [keys[a], keys[b], keys[c], keys[d]]
        uv = {k: [0.0, 0.0] for k in ids}
        faces[str(uuid.uuid4())] = {"uv": uv, "vertices": ids}
    return keys


def main() -> None:
    src = json.loads(SRC.read_text(encoding="utf-8"))
    bones = []
    for e in src["elements"]:
        if e.get("type") != "armature_bone":
            continue
        bone = dict(e)
        bone["vertex_weights"] = {}
        bones.append(bone)
    swatch = Image.new("RGBA", (16, 16), (72, 42, 24, 255))
    buf = io.BytesIO()
    swatch.save(buf, format="PNG")
    data = {
        "meta": {"format_version": src.get("meta", {}).get("format_version", "4.10"),
                 "model_format": "free", "box_uv": False},
        "name": "TestHairSpikes",
        "elements": bones,
        "resolution": {"width": 16, "height": 16},
        "textures": [{
            "name": "hair_swatch",
            "id": "0",
            "source": "data:image/png;base64," + base64.b64encode(buf.getvalue()).decode("ascii"),
        }],
    }
    elements = data["elements"]

    mesh_uuid = str(uuid.uuid4())
    vertices: dict[str, list[float]] = {}
    faces: dict[str, dict] = {}
    all_keys: list[str] = []
    # Local to HeadMesh origin [0, 33, 0]. Scalp top is ~y 8.
    all_keys += add_box(vertices, faces, 0.0, 7.35, 0.15, 6.4, 1.5, 6.2)
    spikes = [
        (0.0, 11.2, -0.4, 1.5, 6.4, 1.5),
        (-2.2, 10.4, 0.6, 1.3, 5.2, 1.3),
        (2.2, 10.4, 0.6, 1.3, 5.2, 1.3),
        (-1.4, 9.6, 2.2, 1.2, 4.2, 1.2),
        (1.4, 9.6, 2.2, 1.2, 4.2, 1.2),
        (0.0, 9.2, -2.4, 1.4, 3.8, 1.4),
        (-2.6, 8.8, -1.6, 1.1, 3.4, 1.1),
        (2.6, 8.8, -1.6, 1.1, 3.4, 1.1),
    ]
    for cx, cy, cz, sx, sy, sz in spikes:
        all_keys += add_box(vertices, faces, cx, cy, cz, sx, sy, sz)

    mesh = {
        "name": MESH_NAME,
        "color": 8,
        "origin": [0, 33, 0],
        "rotation": [0, 0, 0],
        "vertices": vertices,
        "faces": faces,
        "type": "mesh",
        "uuid": mesh_uuid,
    }
    elements.append(mesh)

    prefix = mesh_uuid[:6]
    weight_keys = {f"{prefix}:{k}": 1.0 for k in all_keys}
    attached = False
    for e in elements:
        if e.get("type") != "armature_bone" or e.get("name") != "Head":
            continue
        vw = dict(e.get("vertex_weights") or {})
        drop = [k for k in vw if k.startswith(prefix + ":")]
        for k in drop:
            del vw[k]
        if not attached:
            vw.update(weight_keys)
            attached = True
        e["vertex_weights"] = vw

    if not attached:
        raise SystemExit("Head bone missing — cannot weight hair")

    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(data, indent="\t") + "\n", encoding="utf-8")
    print(f"wrote {MESH_NAME} verts={len(vertices)} faces={len(faces)} -> {OUT}")


if __name__ == "__main__":
    main()
