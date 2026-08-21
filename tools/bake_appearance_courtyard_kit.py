"""Bake A0-02 appearance-courtyard kit bbmodels (banner, tower stub, weapon rack, workbench)."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from bake_tree_bbmodel import REPO, bake_bbmodel

MODELS = REPO / "samples/open-world-rpg/assets/models"

PROPS = [
    {
        "name": "tessera_banner_pole",
        "src": REPO / "tools/art/tessera-banner-pole/Tessera_Banner_Pole.bbmodel",
        "dst": MODELS / "tessera_banner_pole.gltf",
        "png": MODELS / "tessera_banner_pole.png",
        "target_height": 3.4,
        "mesh_name": "TesseraBannerPole",
        "mat_name": "TesseraBannerPoleAtlas",
        "double_sided": True,
    },
    {
        "name": "corner_tower_stub",
        "src": REPO / "tools/art/corner-tower-stub/Corner_Tower_Stub.bbmodel",
        "dst": MODELS / "corner_tower_stub.gltf",
        "png": MODELS / "corner_tower_stub.png",
        "target_height": 5.4,
        "mesh_name": "CornerTowerStub",
        "mat_name": "CornerTowerStubAtlas",
    },
    {
        "name": "weapon_rack",
        "src": REPO / "tools/art/weapon-rack/Weapon_Rack.bbmodel",
        "dst": MODELS / "weapon_rack.gltf",
        "png": MODELS / "weapon_rack.png",
        "target_height": 2.15,
        "mesh_name": "WeaponRack",
        "mat_name": "WeaponRackAtlas",
        "double_sided": True,
    },
    {
        "name": "workbench_table",
        "src": REPO / "tools/art/workbench-table/Workbench_Table.bbmodel",
        "dst": MODELS / "workbench_table.gltf",
        "png": MODELS / "workbench_table.png",
        "target_height": 1.05,
        "mesh_name": "WorkbenchTable",
        "mat_name": "WorkbenchTableAtlas",
    },
]


def bake_one(prop: dict) -> None:
    src = Path(prop["src"])
    if not src.exists():
        raise SystemExit(f"missing source {src}")
    dst_png = Path(prop["png"])
    bake_bbmodel(
        src,
        Path(prop["dst"]),
        atlas_png=dst_png,
        atlas_uri=dst_png.name,
        target_height=float(prop["target_height"]),
        mesh_name=str(prop["mesh_name"]),
        mat_name=str(prop["mat_name"]),
        double_sided=bool(prop.get("double_sided", False)),
        generator=(
            f"AI RPG Engine {prop['name']} bake from {src.name} "
            "v1-appearance-courtyard"
        ),
    )
    art_png = src.with_suffix(".png")
    if dst_png.exists() and art_png.resolve() != dst_png.resolve():
        art_png.write_bytes(dst_png.read_bytes())


def main() -> None:
    parser = argparse.ArgumentParser(description="Bake appearance courtyard kit bbmodels")
    parser.add_argument("names", nargs="*", help="Optional prop name filter")
    args = parser.parse_args()
    wanted = {n.strip() for n in args.names if n.strip()}
    baked = 0
    for prop in PROPS:
        if wanted and prop["name"] not in wanted:
            continue
        bake_one(prop)
        baked += 1
    if wanted and baked == 0:
        known = ", ".join(p["name"] for p in PROPS)
        raise SystemExit(f"no matching props; known: {known}")


if __name__ == "__main__":
    sys.exit(main() or 0)
