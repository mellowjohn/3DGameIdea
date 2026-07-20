"""Derive oak canopy variants from tools/art/tree/Tree.bbmodel and bake glTFs.

Variants keep the same atlas (tree.png) so materials stay consistent with the
in-scene oak. Re-run after editing the base Tree.bbmodel.
"""

from __future__ import annotations

import copy
import json
import math
import uuid
from pathlib import Path

from bake_tree_bbmodel import REPO, bake_bbmodel

BASE = REPO / "tools/art/tree/Tree.bbmodel"
VARIANT_DIR = REPO / "tools/art/tree/variants"
MODELS = REPO / "samples/open-world-rpg/assets/models"
PREFABS = REPO / "samples/open-world-rpg/assets/prefabs/Scene Assets"


def new_uuid() -> str:
    return str(uuid.uuid4())


def scale_verts(element: dict, sx: float, sy: float, sz: float) -> None:
    verts = element.get("vertices") or {}
    for key, pos in list(verts.items()):
        verts[key] = [pos[0] * sx, pos[1] * sy, pos[2] * sz]


def offset_origin(element: dict, dx: float, dy: float, dz: float) -> None:
    o = element.get("origin") or [0.0, 0.0, 0.0]
    element["origin"] = [o[0] + dx, o[1] + dy, o[2] + dz]


def set_origin_xz_scale(element: dict, scale: float) -> None:
    o = element.get("origin") or [0.0, 0.0, 0.0]
    element["origin"] = [o[0] * scale, o[1], o[2] * scale]


def rotate_origin_y(element: dict, degrees: float) -> None:
    o = element.get("origin") or [0.0, 0.0, 0.0]
    rad = math.radians(degrees)
    c, s = math.cos(rad), math.sin(rad)
    x, z = o[0], o[2]
    element["origin"] = [x * c - z * s, o[1], x * s + z * c]


def is_trunk(element: dict) -> bool:
    return (element.get("name") or "").lower() == "cylinder"


def canopy_elements(model: dict) -> list[dict]:
    return [e for e in model["elements"] if not is_trunk(e)]


def trunk_element(model: dict) -> dict:
    for e in model["elements"]:
        if is_trunk(e):
            return e
    raise RuntimeError("missing trunk cylinder")


def remuuid(model: dict) -> None:
    """Give every element a fresh uuid and rebuild outliner."""
    ids = []
    for e in model["elements"]:
        e["uuid"] = new_uuid()
        ids.append(e["uuid"])
    model["outliner"] = ids
    model["groups"] = []


def make_wide(base: dict) -> dict:
    m = copy.deepcopy(base)
    m["name"] = "Oak Wide"
    trunk = trunk_element(m)
    scale_verts(trunk, 1.18, 0.92, 1.18)
    for e in canopy_elements(m):
        set_origin_xz_scale(e, 1.38)
        scale_verts(e, 1.12, 1.05, 1.12)
        offset_origin(e, 0.0, -1.5, 0.0)
    remuuid(m)
    return m


def make_tall(base: dict) -> dict:
    m = copy.deepcopy(base)
    m["name"] = "Oak Tall"
    trunk = trunk_element(m)
    scale_verts(trunk, 0.92, 1.35, 0.92)
    for e in canopy_elements(m):
        o = e["origin"]
        e["origin"] = [o[0] * 0.95, o[1] + 10.0, o[2] * 0.95]
        scale_verts(e, 0.95, 1.08, 0.95)
    remuuid(m)
    return m


def make_lean(base: dict) -> dict:
    m = copy.deepcopy(base)
    m["name"] = "Oak Lean"
    trunk = trunk_element(m)
    trunk["rotation"] = [0.0, 0.0, 8.0]
    for e in canopy_elements(m):
        offset_origin(e, 6.0, 0.0, -2.0)
        rotate_origin_y(e, 18.0)
        scale_verts(e, 1.02, 0.98, 1.02)
    remuuid(m)
    return m


def make_asymmetric(base: dict) -> dict:
    m = copy.deepcopy(base)
    m["name"] = "Oak Asymmetric"
    # Drop two small canopy clusters for a chewed silhouette.
    keep = []
    dropped = 0
    for e in m["elements"]:
        if is_trunk(e):
            keep.append(e)
            continue
        o = e["origin"]
        # Prefer dropping the smaller forward puff and one side puff.
        if dropped < 2 and (abs(o[0]) > 7 or o[2] > 12):
            dropped += 1
            continue
        keep.append(e)
    m["elements"] = keep
    for i, e in enumerate(canopy_elements(m)):
        if i % 2 == 0:
            offset_origin(e, 4.5, 1.0, -3.0)
            scale_verts(e, 1.15, 1.1, 1.15)
        else:
            offset_origin(e, -3.0, -2.0, 4.0)
            scale_verts(e, 0.88, 0.92, 0.88)
    remuuid(m)
    return m


def make_young(base: dict) -> dict:
    m = copy.deepcopy(base)
    m["name"] = "Oak Young"
    trunk = trunk_element(m)
    scale_verts(trunk, 0.72, 0.85, 0.72)
    for e in canopy_elements(m):
        set_origin_xz_scale(e, 0.72)
        o = e["origin"]
        e["origin"] = [o[0], o[1] * 0.88, o[2]]
        scale_verts(e, 0.78, 0.82, 0.78)
    remuuid(m)
    return m


def write_prefab(stem: str, mesh_rel: str) -> None:
    prefab = {
        "schemaVersion": 2,
        "entities": [
            {
                "name": stem.replace("_", " ").title(),
                "transform": {
                    "position": [0.0, 0.0, 0.0],
                    "rotation": [0.0, 0.0, 0.0, 1.0],
                    "scale": [1.0, 1.0, 1.0],
                },
                "parent": None,
                "mesh": {"asset": mesh_rel},
            }
        ],
    }
    path = PREFABS / f"{stem}.prefab.json"
    path.write_text(json.dumps(prefab, indent=2) + "\n", encoding="utf-8")
    meta = {"dependencies": [mesh_rel]}
    path.with_suffix(path.suffix + ".meta").write_text(
        json.dumps(meta, indent=2) + "\n", encoding="utf-8"
    )
    print(f"wrote {path}")


VARIANTS = [
    ("oak_wide", make_wide, 2.85),
    ("oak_tall", make_tall, 3.55),
    ("oak_lean", make_lean, 3.05),
    ("oak_asymmetric", make_asymmetric, 2.95),
    ("oak_young", make_young, 2.25),
]


def main() -> None:
    base = json.loads(BASE.read_text(encoding="utf-8"))
    VARIANT_DIR.mkdir(parents=True, exist_ok=True)
    MODELS.mkdir(parents=True, exist_ok=True)

    for stem, factory, height in VARIANTS:
        model = factory(base)
        bb = VARIANT_DIR / f"{stem}.bbmodel"
        bb.write_text(json.dumps(model, separators=(",", ":")), encoding="utf-8")
        print(f"wrote {bb}")
        dst = MODELS / f"{stem}.gltf"
        bake_bbmodel(
            bb,
            dst,
            atlas_png=MODELS / "tree.png",
            write_atlas=False,
            atlas_uri="tree.png",
            target_height=height,
            mesh_name=stem.replace("_", " ").title().replace(" ", ""),
        )
        write_prefab(stem, f"assets/models/{stem}.gltf")

    print(f"done — {len(VARIANTS)} oak variants")


if __name__ == "__main__":
    main()
