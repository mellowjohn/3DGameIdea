#!/usr/bin/env python3
"""Smoke tests for asset_bake_verify clip / regress gates."""
from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))

import asset_bake_verify as verify  # noqa: E402


def minimal_gltf(anims: list[str]) -> dict:
    # Tiny valid-ish glTF with empty buffer and one mesh prim (no real geo — empty mesh gate will fail;
    # we only assert clip gate helpers here via direct function use).
    return {
        "asset": {"version": "2.0", "generator": "test"},
        "buffers": [{"byteLength": 4, "uri": "data:application/octet-stream;base64,AAAAAA=="}],
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 4}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 1, "type": "VEC3", "min": [0, 0, 0], "max": [0, 1, 0]},
            {"bufferView": 0, "componentType": 5123, "count": 3, "type": "SCALAR"},
        ],
        "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "indices": 1}]}],
        "nodes": [{"name": "Hips"}, {"mesh": 0}],
        "scenes": [{"nodes": [1]}],
        "scene": 0,
        "animations": [{"name": n, "samplers": [], "channels": []} for n in anims],
    }


def test_clip_names():
    g = minimal_gltf(["Idle", "Run"])
    assert verify.clip_names(g) == ["Idle", "Run"]


def test_animator_parse(tmp_path: Path):
    anim = {
        "layers": [
            {
                "states": [
                    {"motion": {"type": "clip", "clip": "Idle"}},
                    {
                        "motion": {
                            "type": "blendTree1D",
                            "children": [
                                {"clip": "Walk"},
                                {"clip": "Run"},
                            ],
                        }
                    },
                ]
            }
        ]
    }
    path = tmp_path / "a.animator.json"
    path.write_text(json.dumps(anim), encoding="utf-8")
    names = verify.animator_clip_names(path)
    assert set(names) == {"Idle", "Walk", "Run"}


def main() -> int:
    test_clip_names()
    with tempfile.TemporaryDirectory() as td:
        test_animator_parse(Path(td))
    # CLIP-MISSING gate via verify_bake with stripped clips
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        models = root / "assets" / "models"
        models.mkdir(parents=True)
        mesh = models / "player.gltf"
        g = minimal_gltf(["Idle"])  # missing Walk/Run/Fall
        # Need skins + joints attrs for skinned path — add stubs
        g["skins"] = [{"joints": [0]}]
        g["meshes"][0]["primitives"][0]["attributes"]["JOINTS_0"] = 0
        g["meshes"][0]["primitives"][0]["attributes"]["WEIGHTS_0"] = 0
        g["asset"]["generator"] = "AI RPG Engine bake_player_v2_gltf.py (test)"
        mesh.write_text(json.dumps(g), encoding="utf-8")
        # 64x64 blank atlas
        from PIL import Image

        atlas = models / "player.png"
        Image.new("RGBA", (64, 64), (128, 100, 80, 255)).save(atlas)

        anim = root / "assets" / "animators"
        anim.mkdir(parents=True)
        (anim / "player.animator.json").write_text(
            json.dumps(
                {
                    "layers": [
                        {
                            "states": [
                                {"motion": {"type": "clip", "clip": "Idle"}},
                                {"motion": {"type": "clip", "clip": "Walk"}},
                                {"motion": {"type": "clip", "clip": "Run"}},
                                {"motion": {"type": "clip", "clip": "Fall"}},
                            ]
                        }
                    ]
                }
            ),
            encoding="utf-8",
        )
        target = {
            "kind": "skinned",
            "animator": "assets/animators/player.animator.json",
            "generatorContains": "bake_player_v2_gltf",
            "verify": {
                "requiredClips": ["Idle", "Walk", "Run", "Fall"],
                "targetHeight": 1.0,
                "heightTolerance": 2.0,
                "feetEpsilon": 1.0,
                "atlasMin": 16,
                "atlasMax": 512,
                "checkSkin": True,
            },
        }
        # Source with more clips than bake
        src = root / "src.gltf"
        src.write_text(json.dumps(minimal_gltf(["Idle", "Walk", "Run", "Fall"])), encoding="utf-8")
        gates = verify.verify_bake(
            project_root=root,
            target=target,
            source_gltf=src,
            baked_mesh=mesh,
            baked_atlas=atlas,
        )
        codes = {g["code"]: g["ok"] for g in gates}
        assert codes.get("ASSET-BAKE-CLIP-MISSING") is False, codes
        assert codes.get("ASSET-BAKE-CLIP-REGRESS") is False, codes
        print("test_asset_bake_verify: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
