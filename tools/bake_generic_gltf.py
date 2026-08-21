"""Bake a skinned glTF (skeleton + animation clips) for any registered generic target.

The Tier-1 prop baker flattens node transforms into one primitive, which destroys skinning, so
skinned imports need their own path. This mirrors the normalize step
`bake_player_v2_gltf.bake` performs — scale positions, scale node translations, offset the
skeleton root, recompute inverse bind matrices, and scale every animation translation track —
but without the player-specific atlas cleanup and hand-winding fixes.

Every animation in the source survives the bake; `asset_bake_verify` gates that with
ASSET-BAKE-CLIP-REGRESS.

Opt-in prop face fixes (weapons like outrider shortbow) match Tier-1 static behavior:
`fix_winding` flips majority-inward prims; `double_sided_thin` emits reverse tri winding for
cord-thin primitives (bow string). The prop pipeline culls BACK faces, so undoubled string
disappears from one side and inverted stave faces leave holes.
"""

from __future__ import annotations

import json
import struct
from pathlib import Path
from typing import Any

import gltf_normalize
from bake_player_v2_gltf import (
    joint_world_matrices,
    mat4_invert,
    read_accessor,
    read_blob,
    write_blob,
    write_f32_accessor,
    write_u16_accessor,
)


def _position_accessors(gltf: dict) -> list[int]:
    found: list[int] = []
    for mesh in gltf.get("meshes") or []:
        for prim in mesh.get("primitives") or []:
            index = (prim.get("attributes") or {}).get("POSITION")
            if index is not None and index not in found:
                found.append(index)
    return found


def _pad4(n: int) -> int:
    return (4 - (n % 4)) % 4


def _prim_winding_stats(pos: list, idx: list[int]) -> tuple[int, int, list]:
    used = sorted({int(i) for i in idx}) if idx else list(range(len(pos)))
    used_pos = [pos[i] for i in used] if used else list(pos)
    if not used_pos:
        return 0, 0, used_pos
    cx = sum(p[0] for p in used_pos) / len(used_pos)
    cy = sum(p[1] for p in used_pos) / len(used_pos)
    cz = sum(p[2] for p in used_pos) / len(used_pos)
    outward = inward = 0
    for i in range(0, len(idx) - 2, 3):
        a = pos[int(idx[i])]
        b = pos[int(idx[i + 1])]
        c = pos[int(idx[i + 2])]
        ab = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
        ac = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
        nx = ab[1] * ac[2] - ab[2] * ac[1]
        ny = ab[2] * ac[0] - ab[0] * ac[2]
        nz = ab[0] * ac[1] - ab[1] * ac[0]
        cen = ((a[0] + b[0] + c[0]) / 3.0, (a[1] + b[1] + c[1]) / 3.0, (a[2] + b[2] + c[2]) / 3.0)
        if nx * (cen[0] - cx) + ny * (cen[1] - cy) + nz * (cen[2] - cz) >= 0.0:
            outward += 1
        else:
            inward += 1
    return outward, inward, used_pos


def _prim_is_thin(used_pos: list, aspect_limit: float = 0.03, uniq_limit: int = 48) -> bool:
    if len(used_pos) < 3:
        return False
    xs = [p[0] for p in used_pos]
    ys = [p[1] for p in used_pos]
    zs = [p[2] for p in used_pos]
    ext = (max(xs) - min(xs), max(ys) - min(ys), max(zs) - min(zs))
    max_ext = max(ext) if ext else 0.0
    min_ext = min(e for e in ext if e > 1e-9) if any(e > 1e-9 for e in ext) else 0.0
    uniq_count = len({(round(p[0], 5), round(p[1], 5), round(p[2], 5)) for p in used_pos})
    aspect = (min_ext / max_ext) if max_ext > 1e-9 else 1.0
    return aspect < aspect_limit or (uniq_count <= uniq_limit and aspect < 0.15)


def _append_index_accessor(gltf: dict, raw: bytearray, indices: list[int]) -> int:
    """Append UNSIGNED_SHORT indices and return the new accessor index."""
    if max(indices, default=0) > 65535:
        raise RuntimeError("skinned bake double-side indices exceed UNSIGNED_SHORT")
    pad = _pad4(len(raw))
    raw.extend(b"\x00" * pad)
    byte_offset = len(raw)
    payload = b"".join(struct.pack("<H", int(i)) for i in indices)
    raw.extend(payload)
    raw.extend(b"\x00" * _pad4(len(payload)))
    gltf.setdefault("bufferViews", []).append(
        {
            "buffer": 0,
            "byteOffset": byte_offset,
            "byteLength": len(payload),
            "target": 34963,
        }
    )
    bv_index = len(gltf["bufferViews"]) - 1
    gltf.setdefault("accessors", []).append(
        {
            "bufferView": bv_index,
            "componentType": 5123,
            "count": len(indices),
            "type": "SCALAR",
        }
    )
    return len(gltf["accessors"]) - 1


def fix_skinned_prop_faces(
    gltf: dict,
    raw: bytearray,
    *,
    fix_winding: bool = False,
    double_sided_thin: bool = False,
) -> dict[str, int]:
    """Flip majority-inward prims and/or dual-wind thin cord prims. Mutates raw/gltf in place."""
    flipped_prims = 0
    doubled_prims = 0
    for mesh in gltf.get("meshes") or []:
        for prim in mesh.get("primitives") or []:
            attrs = prim.get("attributes") or {}
            if "POSITION" not in attrs or "indices" not in prim:
                continue
            pos = read_accessor(gltf, raw, attrs["POSITION"])
            idx = [int(v) for v in read_accessor(gltf, raw, prim["indices"])]
            if len(idx) < 3:
                continue
            outward, inward, used_pos = _prim_winding_stats(pos, idx)
            did_flip = False
            if fix_winding and inward > outward:
                for i in range(0, len(idx) - 2, 3):
                    idx[i + 1], idx[i + 2] = idx[i + 2], idx[i + 1]
                flipped_prims += 1
                did_flip = True
            thin = double_sided_thin and _prim_is_thin(used_pos)
            if thin:
                doubled = list(idx)
                for i in range(0, len(idx) - 2, 3):
                    doubled.append(idx[i])
                    doubled.append(idx[i + 2])
                    doubled.append(idx[i + 1])
                prim["indices"] = _append_index_accessor(gltf, raw, doubled)
                doubled_prims += 1
            elif did_flip:
                acc = gltf["accessors"][prim["indices"]]
                if acc.get("componentType") == 5123:
                    write_u16_accessor(gltf, raw, prim["indices"], idx)
                else:
                    prim["indices"] = _append_index_accessor(gltf, raw, idx)
    return {"flippedPrims": flipped_prims, "doubledThinPrims": doubled_prims}


def bake_skinned(
    *,
    source: Path,
    mesh_out: Path,
    atlas_out: Path,
    generator: str,
    target_height: float | None = None,
    uniform_scale: float | None = None,
    uniform_offset: tuple[float, float, float] | None = None,
    fix_winding: bool = False,
    double_sided_thin: bool = False,
    clean_backdrop: bool = False,
) -> dict[str, Any]:
    """Write `mesh_out` + `atlas_out` from a skinned source glTF/GLB. Returns bake metadata."""
    gltf, buffers = gltf_normalize.load_source(source)
    atlas = gltf_normalize.resolve_image(gltf, buffers, source)
    blob = gltf_normalize.merge_buffers(gltf, buffers)
    gltf_normalize.embed_buffer(gltf, blob)
    raw = read_blob(gltf)

    source_clips = [a.get("name") for a in gltf.get("animations") or []]

    mesh_out.parent.mkdir(parents=True, exist_ok=True)
    if atlas is None:
        raise RuntimeError(f"{source}: no texture found (expected embedded image or sidecar PNG)")

    if clean_backdrop:
        # Reuse Tier-1 Blockbench backdrop punch + RGB inpaint so OPAQUE materials do not show
        # transparent black gutters as holes.
        from collections import deque

        from bake_tier1_props_gltf import clean_blockbench_atlas

        atlas = clean_blockbench_atlas(atlas, foliage=False)
        tex = atlas.convert("RGBA")
        tw, th = tex.size
        opaque = [(x, y) for y in range(th) for x in range(tw) if tex.getpixel((x, y))[3] >= 8]
        nearest = [[None] * tw for _ in range(th)]
        if opaque:
            q = deque()
            for x, y in opaque:
                nearest[y][x] = (x, y)
                q.append((x, y))
            while q:
                x, y = q.popleft()
                src_xy = nearest[y][x]
                for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                    xx, yy = x + dx, y + dy
                    if 0 <= xx < tw and 0 <= yy < th and nearest[yy][xx] is None:
                        nearest[yy][xx] = src_xy
                        q.append((xx, yy))
            px = tex.load()
            for y in range(th):
                for x in range(tw):
                    if px[x, y][3] >= 8:
                        continue
                    hit = nearest[y][x]
                    if hit is None:
                        px[x, y] = (40, 70, 30, 255)
                    else:
                        r, g, b, _ = px[hit[0], hit[1]]
                        px[x, y] = (r, g, b, 255)
        atlas = tex

    atlas.save(atlas_out, format="PNG")
    gltf["images"] = [{"uri": atlas_out.name}]
    gltf["textures"] = [{"source": 0, "sampler": 0}]
    gltf.setdefault("samplers", [{"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 33071}])
    for material in gltf.get("materials") or []:
        material["alphaMode"] = "OPAQUE"
        material.pop("alphaCutoff", None)
        if fix_winding or double_sided_thin:
            material["doubleSided"] = True
        pbr = material.setdefault("pbrMetallicRoughness", {})
        pbr["baseColorTexture"] = {"index": 0}
        pbr["metallicFactor"] = 0
        pbr["roughnessFactor"] = 1
        pbr.pop("baseColorFactor", None)

    position_accessors = _position_accessors(gltf)
    if not position_accessors:
        raise RuntimeError(f"{source}: no POSITION attribute found")

    positions = {index: read_accessor(gltf, raw, index) for index in position_accessors}
    all_points = [p for values in positions.values() for p in values]
    xs = [p[0] for p in all_points]
    ys = [p[1] for p in all_points]
    zs = [p[2] for p in all_points]
    raw_height = max(ys) - min(ys)

    # `joint_world_matrices` composes TRS only. A source that stores baked node matrices cannot be
    # rescaled without decomposing them, so leave such a model at authored scale instead of
    # silently corrupting its bind poses.
    matrix_nodes = any("matrix" in node for node in gltf.get("nodes") or [])
    matched_player = uniform_scale is not None
    if matched_player and matrix_nodes:
        raise RuntimeError(f"{source}: matchPlayerBake cannot rescale nodes that store baked matrices")
    normalize_scale = (matched_player or bool(target_height)) and not matrix_nodes

    scale = 1.0
    offset = (0.0, 0.0, 0.0)
    if normalize_scale:
        if matched_player:
            scale = float(uniform_scale)
            offset = uniform_offset or (0.0, 0.0, 0.0)
        else:
            if raw_height > 1e-6:
                scale = float(target_height) / raw_height
            center_x = 0.5 * (min(xs) + max(xs))
            center_z = 0.5 * (min(zs) + max(zs))
            offset = (-scale * center_x, -scale * min(ys), -scale * center_z)

        for index, values in positions.items():
            write_f32_accessor(
                gltf,
                raw,
                index,
                [(scale * x + offset[0], scale * y + offset[1], scale * z + offset[2]) for x, y, z in values],
            )

    skins = gltf.get("skins") or []
    if normalize_scale:
        skeleton_roots = {skin["skeleton"] for skin in skins if skin.get("skeleton") is not None}
        for node_index, node in enumerate(gltf.get("nodes") or []):
            if "translation" in node:
                x, y, z = node["translation"]
                node["translation"] = [scale * x, scale * y, scale * z]
            if node_index in skeleton_roots:
                translation = node.get("translation", [0.0, 0.0, 0.0])
                node["translation"] = [
                    translation[0] + offset[0],
                    translation[1] + offset[1],
                    translation[2] + offset[2],
                ]

        # A skinned mesh node's transform is ignored by the spec; clearing it avoids a double transform.
        for node in gltf.get("nodes") or []:
            if "mesh" in node and "skin" in node:
                node.pop("translation", None)
                node.pop("rotation", None)
                node.pop("scale", None)

        if skins:
            worlds = joint_world_matrices(gltf)
            for skin in skins:
                ibm_accessor = skin.get("inverseBindMatrices")
                if ibm_accessor is None:
                    continue
                write_f32_accessor(
                    gltf, raw, ibm_accessor, [mat4_invert(worlds[joint]) for joint in skin["joints"]]
                )

        for animation in gltf.get("animations") or []:
            for channel in animation.get("channels") or []:
                if (channel.get("target") or {}).get("path") != "translation":
                    continue
                output = animation["samplers"][channel["sampler"]]["output"]
                values = read_accessor(gltf, raw, output)
                write_f32_accessor(
                    gltf, raw, output, [(scale * x, scale * y, scale * z) for x, y, z in values]
                )

    face_meta = fix_skinned_prop_faces(
        gltf, raw, fix_winding=fix_winding, double_sided_thin=double_sided_thin
    )

    write_blob(gltf, bytes(raw))
    gltf.setdefault("asset", {})["generator"] = generator
    gltf["asset"]["version"] = "2.0"
    mesh_out.write_text(json.dumps(gltf, separators=(",", ":")), encoding="utf-8")

    baked_clips = [a.get("name") for a in gltf.get("animations") or []]
    return {
        "source": str(source),
        "mesh": str(mesh_out),
        "atlas": str(atlas_out),
        "generator": generator,
        "sourceClips": source_clips,
        "bakedClips": baked_clips,
        "scale": scale,
        "offset": list(offset),
        "rawHeight": raw_height,
        "targetHeight": target_height if normalize_scale and not matched_player else None,
        "normalized": normalize_scale,
        "matchedPlayerBake": matched_player,
        "joints": sum(len(skin.get("joints") or []) for skin in skins),
        **face_meta,
    }


def main() -> int:
    import argparse

    parser = argparse.ArgumentParser(description="Bake a skinned glTF for the engine")
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--mesh-out", type=Path, required=True)
    parser.add_argument("--atlas-out", type=Path, required=True)
    parser.add_argument("--generator", type=str, default="AI RPG Engine generic skinned bake")
    parser.add_argument("--target-height", type=float, default=None)
    parser.add_argument("--fix-winding", action="store_true")
    parser.add_argument("--double-sided-thin", action="store_true")
    parser.add_argument("--clean-backdrop", action="store_true")
    args = parser.parse_args()
    meta = bake_skinned(
        source=args.source,
        mesh_out=args.mesh_out,
        atlas_out=args.atlas_out,
        generator=args.generator,
        target_height=args.target_height,
        fix_winding=args.fix_winding,
        double_sided_thin=args.double_sided_thin,
        clean_backdrop=args.clean_backdrop,
    )
    print(json.dumps(meta, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
