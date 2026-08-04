"""Fail-closed post-bake verification for asset_bake.py."""
from __future__ import annotations

import base64
import json
import struct
from pathlib import Path
from typing import Any

from PIL import Image

# Blockbench yellow canvas (recurring player failure class).
CANVAS_YELLOW = (248, 221, 114)
SOFT_BLUE = (180, 212, 225)
NEON_HUES = [
    (0, 255, 0),
    (255, 0, 255),
    (0, 255, 255),
    (255, 0, 0),
    (255, 255, 0),
]


def gate(code: str, ok: bool, detail: str, remediation: str = "") -> dict[str, Any]:
    out: dict[str, Any] = {"code": code, "ok": bool(ok), "detail": detail}
    if remediation and not ok:
        out["remediation"] = remediation
    return out


def load_gltf(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def read_blob(g: dict) -> bytes:
    uri = g["buffers"][0]["uri"]
    if uri.startswith("data:application/octet-stream;base64,"):
        return base64.b64decode(uri.split(",", 1)[1])
    raise RuntimeError("unsupported buffer uri")


def read_accessor(g: dict, raw: bytes, acc_idx: int):
    acc = g["accessors"][acc_idx]
    bv = g["bufferViews"][acc["bufferView"]]
    offset = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    ctype = acc["componentType"]
    typ = acc["type"]
    comps = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[typ]
    size = {5126: 4, 5123: 2, 5121: 1, 5125: 4}[ctype]
    stride = bv.get("byteStride", size * comps)
    fmt = {5126: "f", 5123: "H", 5121: "B", 5125: "I"}[ctype]
    out = []
    for i in range(acc["count"]):
        o = offset + i * stride
        vals = struct.unpack_from("<" + fmt * comps, raw, o)
        out.append(vals if comps > 1 else vals[0])
    return out


def clip_names(g: dict) -> list[str]:
    return [a.get("name") or f"anim_{i}" for i, a in enumerate(g.get("animations") or [])]


def clip_duration(g: dict, raw: bytes, anim: dict) -> float:
    best = 0.0
    for sampler in anim.get("samplers") or []:
        times = read_accessor(g, raw, sampler["input"])
        if times:
            best = max(best, float(times[-1]))
    return best


def mesh_aabb(g: dict, raw: bytes):
    prim = g["meshes"][0]["primitives"][0]
    pos = read_accessor(g, raw, prim["attributes"]["POSITION"])
    xs = [p[0] for p in pos]
    ys = [p[1] for p in pos]
    zs = [p[2] for p in pos]
    return {
        "min": [min(xs), min(ys), min(zs)],
        "max": [max(xs), max(ys), max(zs)],
        "count": len(pos),
        "height": max(ys) - min(ys),
        "span_xz": max(max(xs) - min(xs), max(zs) - min(zs)),
        "span_extent": max(max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs)),
    }


def joint_names(g: dict) -> list[str]:
    names = []
    for skin in g.get("skins") or []:
        for ji in skin.get("joints") or []:
            names.append(g["nodes"][ji].get("name") or f"node_{ji}")
    return names


_HAND_JOINT_TOKENS = ("Hand", "Thumb", "Index", "Middle", "Ring", "Pinky")


def hand_winding_stats(g: dict, raw: bytes) -> dict[str, Any]:
    """Count outward vs inward tris for hand/digit-dominated vertices."""
    if not (g.get("meshes") and g.get("skins")):
        return {"hand_tris": 0, "outward": 0, "inward": 0}
    prim = g["meshes"][0]["primitives"][0]
    attrs = prim.get("attributes") or {}
    if "POSITION" not in attrs or "JOINTS_0" not in attrs or "WEIGHTS_0" not in attrs:
        return {"hand_tris": 0, "outward": 0, "inward": 0}
    if "indices" not in prim:
        return {"hand_tris": 0, "outward": 0, "inward": 0}

    pos = read_accessor(g, raw, attrs["POSITION"])
    joints_attr = read_accessor(g, raw, attrs["JOINTS_0"])
    weights_attr = read_accessor(g, raw, attrs["WEIGHTS_0"])
    idx = [int(i) for i in read_accessor(g, raw, prim["indices"])]
    skin_joints = g["skins"][0]["joints"]
    names = [g["nodes"][ji].get("name") or "" for ji in skin_joints]
    hand_ids = {
        i for i, name in enumerate(names) if any(tok in name for tok in _HAND_JOINT_TOKENS)
    }
    if not hand_ids:
        return {"hand_tris": 0, "outward": 0, "inward": 0}

    def dominant(jv, wv) -> int:
        best_i, best_w = 0, -1.0
        for k in range(4):
            w = float(wv[k])
            j = int(jv[k])
            if w > best_w:
                best_w, best_i = w, j
        return best_i

    outward = inward = hand_tris = 0
    for ji in hand_ids:
        verts = [
            vi
            for vi, (jv, wv) in enumerate(zip(joints_attr, weights_attr))
            if dominant(jv, wv) == ji
        ]
        if len(verts) < 3:
            continue
        vset = set(verts)
        cx = sum(pos[v][0] for v in verts) / len(verts)
        cy = sum(pos[v][1] for v in verts) / len(verts)
        cz = sum(pos[v][2] for v in verts) / len(verts)
        for t in range(0, len(idx) - 2, 3):
            a, b, c = idx[t], idx[t + 1], idx[t + 2]
            if a not in vset or b not in vset or c not in vset:
                continue
            hand_tris += 1
            ax, ay, az = pos[a]
            bx, by, bz = pos[b]
            cx2, cy2, cz2 = pos[c]
            ab = (bx - ax, by - ay, bz - az)
            ac = (cx2 - ax, cy2 - ay, cz2 - az)
            nx = ab[1] * ac[2] - ab[2] * ac[1]
            ny = ab[2] * ac[0] - ab[0] * ac[2]
            nz = ab[0] * ac[1] - ab[1] * ac[0]
            fx = (ax + bx + cx2) / 3.0
            fy = (ay + by + cy2) / 3.0
            fz = (az + bz + cz2) / 3.0
            if nx * (fx - cx) + ny * (fy - cy) + nz * (fz - cz) >= 0.0:
                outward += 1
            else:
                inward += 1
    return {"hand_tris": hand_tris, "outward": outward, "inward": inward}


def loco_clip_channel_count(g: dict, clip: str) -> int:
    for anim in g.get("animations") or []:
        if anim.get("name") == clip:
            return len(anim.get("channels") or [])
    return 0


def animator_clip_names(animator_path: Path) -> list[str]:
    if not animator_path.exists():
        return []
    data = json.loads(animator_path.read_text(encoding="utf-8"))
    found: set[str] = set()

    def walk(obj):
        if isinstance(obj, dict):
            if obj.get("type") == "clip" and obj.get("clip"):
                found.add(str(obj["clip"]))
            for child in obj.get("children") or []:
                if isinstance(child, dict) and child.get("clip"):
                    found.add(str(child["clip"]))
            for v in obj.values():
                walk(v)
        elif isinstance(obj, list):
            for v in obj:
                walk(v)

    walk(data)
    return sorted(found)


def rig_joint_names(rig_path: Path) -> list[str]:
    if not rig_path.exists():
        return []
    data = json.loads(rig_path.read_text(encoding="utf-8"))
    names: set[str] = set()
    for role in data.get("boneRoles") or []:
        if role.get("jointName"):
            names.add(role["jointName"])
    for hook in data.get("ikHooks") or []:
        for key in ("tipJoint", "rootJoint", "poleJoint"):
            if hook.get(key):
                names.add(hook[key])
    return sorted(names)


def color_near(px, target, tol=18) -> bool:
    return all(abs(int(px[i]) - target[i]) <= tol for i in range(3)) and px[3] > 8


def count_near(img: Image.Image, target, tol=18) -> int:
    n = 0
    for px in img.getdata():
        if color_near(px, target, tol):
            n += 1
    return n


def opaque_unique_colors(img: Image.Image, quant=8) -> int:
    colors = set()
    for r, g, b, a in img.getdata():
        if a < 8:
            continue
        colors.add((r // quant * quant, g // quant * quant, b // quant * quant))
    return len(colors)


def count_transparent_uv_samples(g: dict, raw: bytes, atlas: Image.Image, alpha_thresh=8) -> int:
    tw, th = atlas.size
    pixels = atlas.load()
    bad = 0
    for mesh in g.get("meshes") or []:
        for prim in mesh.get("primitives") or []:
            attrs = prim.get("attributes") or {}
            if "TEXCOORD_0" not in attrs:
                continue
            uvs = read_accessor(g, raw, attrs["TEXCOORD_0"])
            for u, v in uvs:
                x = min(tw - 1, max(0, int(u * tw) % tw))
                y = min(th - 1, max(0, int(v * th) % th))
                if pixels[x, y][3] < alpha_thresh:
                    bad += 1
    return bad


def verify_bake(
    *,
    project_root: Path,
    target: dict,
    source_gltf: Path | None,
    baked_mesh: Path,
    baked_atlas: Path | None,
    source_atlas: Path | None = None,
) -> list[dict[str, Any]]:
    """Return list of gate results. All must be ok for success."""
    results: list[dict[str, Any]] = []
    v = target.get("verify") or {}
    kind = target.get("kind", "static")

    if not baked_mesh.exists():
        return [
            gate(
                "ASSET-BAKE-EMPTY-MESH",
                False,
                f"missing baked mesh {baked_mesh}",
                "Re-run bake and check write permissions / locked engine.exe.",
            )
        ]

    try:
        g = load_gltf(baked_mesh)
        raw = read_blob(g)
    except Exception as exc:  # noqa: BLE001
        return [gate("ASSET-BAKE-EMPTY-MESH", False, f"failed to parse baked glTF: {exc}")]

    # Empty mesh
    try:
        aabb = mesh_aabb(g, raw)
        has_idx = False
        for mesh in g.get("meshes") or []:
            for prim in mesh.get("primitives") or []:
                if "indices" in prim:
                    has_idx = True
        ok_empty = aabb["count"] > 0 and has_idx
        results.append(
            gate(
                "ASSET-BAKE-EMPTY-MESH",
                ok_empty,
                f"verts={aabb['count']} has_indices={has_idx}",
                "Source glTF may be corrupt; re-export from Blockbench.",
            )
        )
    except Exception as exc:  # noqa: BLE001
        results.append(gate("ASSET-BAKE-EMPTY-MESH", False, str(exc)))
        aabb = {"height": 0.0, "span_xz": 0.0, "span_extent": 0.0, "min": [0, 0, 0], "max": [0, 0, 0]}

    # Generator
    gen = (g.get("asset") or {}).get("generator", "")
    needle = target.get("generatorContains") or ""
    if needle:
        results.append(
            gate(
                "ASSET-BAKE-GENERATOR",
                needle in gen,
                f"generator={gen!r}",
                "Bake did not stamp expected generator; wrong baker ran.",
            )
        )
    else:
        results.append(gate("ASSET-BAKE-GENERATOR", True, f"generator={gen!r}"))

    # Height / feet / span. Imports whose sources store baked node matrices cannot be rescaled or
    # feet-normalized without decomposing them, so those targets opt out via checkFeet/checkHeight.
    feet_eps = float(v.get("feetEpsilon", 0.05))
    min_y = float(aabb["min"][1])
    if v.get("checkFeet", True):
        results.append(
            gate(
                "ASSET-BAKE-FEET",
                abs(min_y) <= feet_eps,
                f"minY={min_y:.4f} eps={feet_eps}",
                "Feet normalize failed; check bake scale path.",
            )
        )

    scale_mode = v.get("scaleMode", "height")
    if not v.get("checkHeight", True):
        results.append(gate("ASSET-BAKE-HEIGHT", True, f"height={float(aabb['height']):.4f} (gate disabled)"))
    elif scale_mode == "max_xz":
        target_span = float(v.get("targetSpan", v.get("targetHeight", 1.0)))
        tol = float(v.get("spanTolerance", v.get("heightTolerance", 0.15)))
        span = float(aabb["span_xz"])
        results.append(
            gate(
                "ASSET-BAKE-HEIGHT",
                abs(span - target_span) <= tol,
                f"span_xz={span:.4f} target={target_span} tol={tol}",
                "Scale drift; check catalog targetSpan.",
            )
        )
    elif scale_mode == "max_extent":
        target_span = float(v.get("targetSpan", v.get("targetHeight", 1.0)))
        tol = float(v.get("spanTolerance", v.get("heightTolerance", 0.15)))
        span = float(aabb["span_extent"])
        results.append(
            gate(
                "ASSET-BAKE-HEIGHT",
                abs(span - target_span) <= tol,
                f"span_extent={span:.4f} target={target_span} tol={tol}",
            )
        )
    else:
        target_h = float(v.get("targetHeight", 1.0))
        tol = float(v.get("heightTolerance", 0.12))
        h = float(aabb["height"])
        results.append(
            gate(
                "ASSET-BAKE-HEIGHT",
                abs(h - target_h) <= tol,
                f"height={h:.4f} target={target_h} tol={tol}",
                "Height normalize failed.",
            )
        )

    # Atlas
    if baked_atlas is None:
        baked_atlas = baked_mesh.with_suffix(".png")
    if not baked_atlas.exists():
        results.append(
            gate(
                "ASSET-BAKE-ATLAS-MISSING",
                False,
                f"missing {baked_atlas}",
                "Bake must emit sidecar PNG next to mesh.",
            )
        )
        atlas_img = None
    else:
        try:
            atlas_img = Image.open(baked_atlas).convert("RGBA")
            tw, th = atlas_img.size
            amin = int(v.get("atlasMin", 16))
            amax = int(v.get("atlasMax", 2048))
            size_ok = amin <= tw <= amax and amin <= th <= amax
            results.append(
                gate(
                    "ASSET-BAKE-ATLAS-SIZE",
                    size_ok,
                    f"atlas={tw}x{th} bounds=[{amin},{amax}]",
                    "Reject 1x1 stubs / absurd resolutions; re-export atlas.",
                )
            )
            if source_atlas and source_atlas.exists() and v.get("atlasSizeStrict"):
                sw, sh = Image.open(source_atlas).size
                results.append(
                    gate(
                        "ASSET-BAKE-ATLAS-SIZE",
                        abs(tw - sw) <= 2 and abs(th - sh) <= 2,
                        f"baked={tw}x{th} source={sw}x{sh}",
                    )
                )
        except Exception as exc:  # noqa: BLE001
            results.append(gate("ASSET-BAKE-ATLAS-MISSING", False, str(exc)))
            atlas_img = None

    if atlas_img is not None and v.get("checkCanvasYellow"):
        n = count_near(atlas_img, CANVAS_YELLOW, tol=12)
        results.append(
            gate(
                "ASSET-BAKE-ATLAS-CANVAS",
                n == 0,
                f"yellow_canvas_pixels={n}",
                "Re-export atlas / bump clean; yellow Blockbench canvas remained.",
            )
        )

    if atlas_img is not None and v.get("checkNeon"):
        neon_n = 0
        for hue in NEON_HUES:
            neon_n += count_near(atlas_img, hue, tol=10)
        # Soft neon islands can be small; fail if dense neon leftover.
        results.append(
            gate(
                "ASSET-BAKE-ATLAS-NEON",
                neon_n < 40,
                f"neonish_pixels={neon_n}",
                "Neon overlay islands survived atlas clean.",
            )
        )

    if atlas_img is not None and v.get("checkSoftBlue") and source_atlas and source_atlas.exists():
        try:
            src_img = Image.open(source_atlas).convert("RGBA")
            src_blue = count_near(src_img, SOFT_BLUE, tol=22)
            dst_blue = count_near(atlas_img, SOFT_BLUE, tol=22)
            ok = src_blue == 0 or dst_blue > 0
            results.append(
                gate(
                    "ASSET-BAKE-ATLAS-SOFT-BLUE",
                    ok,
                    f"source_soft_blue={src_blue} baked={dst_blue}",
                    "Soft eye/metal blues wiped by atlas clean.",
                )
            )
        except Exception as exc:  # noqa: BLE001
            results.append(gate("ASSET-BAKE-ATLAS-SOFT-BLUE", False, str(exc)))

    if atlas_img is not None and int(v.get("minOpaqueUniqueColors") or 0) > 0:
        uniq = opaque_unique_colors(atlas_img)
        floor = int(v["minOpaqueUniqueColors"])
        results.append(
            gate(
                "ASSET-BAKE-ATLAS-MUDDY",
                uniq >= floor,
                f"opaque_unique≈{uniq} floor={floor}",
                "Foliage atlas collapsed; use soft foliage clean / re-export paint.",
            )
        )

    if atlas_img is not None and v.get("checkUvTransparent"):
        bad = count_transparent_uv_samples(g, raw, atlas_img)
        results.append(
            gate(
                "ASSET-BAKE-UV-TRANSPARENT",
                bad == 0,
                f"transparent_uv_samples={bad}",
                "UVs hit empty atlas texels; UV snap / paint islands.",
            )
        )

    # Source pairing mismatch (GoodPlayer + V2 atlas)
    if source_gltf and source_atlas:
        if "GoodPlayerModel" in source_gltf.name and "Player_V2" in source_atlas.name:
            results.append(
                gate(
                    "ASSET-BAKE-ATLAS-MISMATCH",
                    False,
                    f"mesh={source_gltf.name} atlas={source_atlas.name}",
                    "Do not pair GoodPlayerModel mesh with legacy Player_V2 atlas.",
                )
            )
        else:
            results.append(gate("ASSET-BAKE-ATLAS-MISMATCH", True, "source mesh/atlas pairing ok"))

    # Skinned gates
    if kind == "skinned" or v.get("checkSkin"):
        skins = g.get("skins") or []
        prim = g["meshes"][0]["primitives"][0]
        attrs = prim.get("attributes") or {}
        skin_ok = bool(skins) and "JOINTS_0" in attrs and "WEIGHTS_0" in attrs
        results.append(
            gate(
                "ASSET-BAKE-SKIN-MISSING",
                skin_ok,
                f"skins={len(skins)} joints_attr={'JOINTS_0' in attrs} weights_attr={'WEIGHTS_0' in attrs}",
                "Re-export skinned glTF from Blockbench with armature.",
            )
        )

        joints = set(joint_names(g))
        rig_path = target.get("rig")
        if rig_path:
            required = set(rig_joint_names(project_root / rig_path))
            missing = sorted(required - joints)
            results.append(
                gate(
                    "ASSET-BAKE-JOINT-MISSING",
                    not missing,
                    f"missing_joints={missing}" if missing else f"joints={len(joints)}",
                    "Joint names must match player.rig.json.",
                )
            )

        # Required clips from catalog + animator
        required_clips = set(v.get("requiredClips") or [])
        anim_path = target.get("animator")
        if anim_path:
            required_clips |= set(animator_clip_names(project_root / anim_path))
        baked_clips = {n: a for n, a in zip(clip_names(g), g.get("animations") or [])}
        missing_clips = sorted(c for c in required_clips if c not in baked_clips)
        results.append(
            gate(
                "ASSET-BAKE-CLIP-MISSING",
                not missing_clips,
                f"missing={missing_clips} have={sorted(baked_clips)}",
                "Re-export glTF with animations; do not overwrite from clip-stripped export.",
            )
        )

        empty = []
        for name in required_clips:
            anim = baked_clips.get(name)
            if not anim:
                continue
            ch = anim.get("channels") or []
            dur = clip_duration(g, raw, anim)
            if not ch or dur <= 0:
                empty.append(name)
        results.append(
            gate(
                "ASSET-BAKE-CLIP-EMPTY",
                not empty,
                f"empty_clips={empty}",
                "Clip present but has no channels/duration.",
            )
        )

        if source_gltf and source_gltf.exists():
            try:
                sg = load_gltf(source_gltf)
                src_names = set(clip_names(sg))
                dropped = sorted(src_names - set(baked_clips))
                results.append(
                    gate(
                        "ASSET-BAKE-CLIP-REGRESS",
                        not dropped,
                        f"dropped_from_source={dropped}",
                        "Bake dropped animations present in source; preserve all clips.",
                    )
                )
            except Exception as exc:  # noqa: BLE001
                results.append(gate("ASSET-BAKE-CLIP-REGRESS", False, str(exc)))

        if v.get("checkWindingHands"):
            stats = hand_winding_stats(g, raw)
            # Fail when hands are majority-inward (D3D backface cull → missing faces).
            ok_wind = stats["hand_tris"] == 0 or stats["inward"] <= stats["outward"]
            results.append(
                gate(
                    "ASSET-BAKE-WINDING-HAND",
                    ok_wind,
                    f"hand_tris={stats['hand_tris']} outward={stats['outward']} inward={stats['inward']}",
                    "Hand/digit faces majority-inward (D3D cull). Rebake with player baker winding flip.",
                )
            )

        min_loco = int(v.get("minLocoChannels", 0) or 0)
        if min_loco > 0:
            loco_bad = []
            for name in ("Walk", "Run", "Fall"):
                if name not in baked_clips:
                    continue
                n = loco_clip_channel_count(g, name)
                if n < min_loco:
                    loco_bad.append(f"{name}={n}")
            results.append(
                gate(
                    "ASSET-BAKE-CLIP-LOCO-THIN",
                    not loco_bad,
                    f"min={min_loco} thin={loco_bad}",
                    "Locomotion clip has too few channels; re-export full armature animation from Blockbench.",
                )
            )

    return results


def all_ok(results: list[dict[str, Any]]) -> bool:
    return all(r.get("ok") for r in results)
