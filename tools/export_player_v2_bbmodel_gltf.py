#!/usr/bin/env python3
"""Export finger-rigged Player_V2_rigged.bbmodel → skinned Player_V2_rigged.gltf.

Reads the current armature (including finger/thumb bones) and mesh UVs from the
bbmodel, writes a glTF suitable for tools/bake_player_v2_gltf.py. Idle is
restored by the bake step from the previous export when needed.
"""
from __future__ import annotations

import base64
import json
import math
import struct
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SRC_BB = Path(r"c:\Users\johnr\Documents\Player_V2_rigged.bbmodel")
SRC_BB_FALLBACK = REPO / "tools/art/player/Player_V2_rigged.bbmodel"
SRC_PNG = Path(r"c:\Users\johnr\Documents\Player_V2.png")
SRC_PNG_FALLBACK = REPO / "tools/art/player/Player_V2.png"
DST_GLTF = REPO / "tools/art/player/Player_V2_rigged.gltf"
DST_GLTF_DOCS = Path(r"c:\Users\johnr\Documents\Player_V2_rigged.gltf")

BB_TO_GLTF = 1.0 / 8.0
UV_SIZE = 64.0


def export(bb_path: Path, png_path: Path | None, out_path: Path) -> None:
    data = json.loads(bb_path.read_text(encoding="utf-8"))
    bones = {
        e["name"]: e
        for e in data["elements"]
        if e.get("type") == "armature_bone" and e.get("name")
    }
    by_uuid = {e["uuid"]: e for e in data["elements"] if e.get("uuid")}
    meshes = [
        e
        for e in data["elements"]
        if e.get("type") not in ("armature", "armature_bone") and "vertices" in e
    ]
    if not bones or "Hips" not in bones:
        raise SystemExit("bbmodel missing Hips armature bone")

    # Hierarchy from bone.children
    hierarchy: dict[str | None, list[str]] = {None: ["Hips"]}
    for name, bone in bones.items():
        kids = []
        for cu in bone.get("children") or []:
            child = by_uuid.get(cu)
            if child and child.get("type") == "armature_bone" and child.get("name") in bones:
                kids.append(child["name"])
        hierarchy[name] = kids

    # Stable bone order: DFS from Hips
    bone_order: list[str] = []

    def walk(n: str) -> None:
        bone_order.append(n)
        for c in hierarchy.get(n) or []:
            walk(c)

    walk("Hips")
    bone_index = {n: i for i, n in enumerate(bone_order)}

    # Weight lookup
    weight_maps = {name: (b.get("vertex_weights") or {}) for name, b in bones.items()}

    def weights_for(mesh_uuid: str, vkey: str):
        key = f"{mesh_uuid[:6]}:{vkey}"
        wmap = {}
        for bname, vw in weight_maps.items():
            if key in vw:
                wmap[bname] = float(vw[key])
        if not wmap:
            wmap = {"Hips": 1.0}
        total = sum(wmap.values()) or 1.0
        items = sorted(((n, w / total) for n, w in wmap.items()), key=lambda t: -t[1])[:4]
        while len(items) < 4:
            items.append(("Hips", 0.0))
        s = sum(w for _, w in items) or 1.0
        return [(n, w / s) for n, w in items]

    positions: list[list[float]] = []
    normals: list[list[float]] = []
    uvs: list[list[float]] = []
    joints0: list[list[int]] = []
    weights0: list[list[float]] = []
    indices: list[int] = []

    for mesh in meshes:
        o = mesh.get("origin") or [0, 0, 0]
        base = len(positions)
        vkeys = list(mesh["vertices"].keys())
        vindex = {k: i for i, k in enumerate(vkeys)}
        for vkey in vkeys:
            local = mesh["vertices"][vkey]
            wpos = [local[i] + o[i] for i in range(3)]
            positions.append([c * BB_TO_GLTF for c in wpos])
            normals.append([0.0, 1.0, 0.0])
            uvs.append([0.0, 0.0])
            witems = weights_for(mesh["uuid"], vkey)
            joints0.append([bone_index[n] for n, _ in witems])
            weights0.append([w for _, w in witems])
        for face in (mesh.get("faces") or {}).values():
            ids = face["vertices"]
            for i in range(1, len(ids) - 1):
                indices.extend(
                    [
                        base + vindex[ids[0]],
                        base + vindex[ids[i]],
                        base + vindex[ids[i + 1]],
                    ]
                )
            uv = face.get("uv") or {}
            for vid in ids:
                if vid in uv and vid in vindex:
                    u, v = uv[vid][:2]
                    # Blockbench mesh UVs are top-left (V+ down), matching D3D sampling.
                    # Do not apply OpenGL/glTF bottom-left V flip — the engine samples V as-is.
                    u, v = float(u) / UV_SIZE, float(v) / UV_SIZE
                    uvs[base + vindex[vid]] = [u, v]

    for t in range(0, len(indices), 3):
        i0, i1, i2 = indices[t], indices[t + 1], indices[t + 2]
        p0, p1, p2 = positions[i0], positions[i1], positions[i2]
        e1 = [p1[j] - p0[j] for j in range(3)]
        e2 = [p2[j] - p0[j] for j in range(3)]
        n = [
            e1[1] * e2[2] - e1[2] * e2[1],
            e1[2] * e2[0] - e1[0] * e2[2],
            e1[0] * e2[1] - e1[1] * e2[0],
        ]
        ln = math.sqrt(sum(c * c for c in n)) or 1.0
        n = [c / ln for c in n]
        for ii in (i0, i1, i2):
            normals[ii] = n

    parent_of = {}
    for parent, kids in hierarchy.items():
        if parent is None:
            continue
        for kid in kids:
            parent_of[kid] = parent

    world_pos: dict[str, list[float]] = {}

    def bone_world(name: str) -> list[float]:
        if name in world_pos:
            return world_pos[name]
        parent = parent_of.get(name)
        local = bones[name].get("origin") or [0, 0, 0]
        if parent is None:
            wp = [local[0] * BB_TO_GLTF, local[1] * BB_TO_GLTF, local[2] * BB_TO_GLTF]
        else:
            pp = bone_world(parent)
            wp = [
                pp[0] + local[0] * BB_TO_GLTF,
                pp[1] + local[1] * BB_TO_GLTF,
                pp[2] + local[2] * BB_TO_GLTF,
            ]
        world_pos[name] = wp
        return wp

    for name in bone_order:
        bone_world(name)

    blob = bytearray()

    def align4() -> None:
        while len(blob) % 4:
            blob.append(0)

    def add_f32(values, comps):
        align4()
        off = len(blob)
        for v in values:
            if comps == 1:
                blob.extend(struct.pack("<f", float(v)))
            else:
                blob.extend(struct.pack("<" + "f" * comps, *[float(x) for x in v]))
        return off

    def add_u16(values):
        align4()
        off = len(blob)
        for v in values:
            blob.extend(struct.pack("<H", int(v)))
        return off

    def add_u8(values, comps):
        align4()
        off = len(blob)
        for v in values:
            blob.extend(struct.pack("<" + "B" * comps, *[int(x) for x in v]))
        return off

    pos_off = add_f32(positions, 3)
    nrm_off = add_f32(normals, 3)
    uv_off = add_f32(uvs, 2)
    j_off = add_u8(joints0, 4)
    w_off = add_f32(weights0, 4)
    idx_off = add_u16(indices)
    ibm = []
    for name in bone_order:
        wp = world_pos[name]
        ibm.append([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -wp[0], -wp[1], -wp[2], 1])
    ibm_off = add_f32(ibm, 16)

    views = []
    accessors = []

    def push_view(offset, length, stride=None):
        v = {"buffer": 0, "byteOffset": offset, "byteLength": length}
        if stride:
            v["byteStride"] = stride
        views.append(v)
        return len(views) - 1

    def push_acc(view, count, type_, ctype, mn=None, mx=None):
        a = {"bufferView": view, "componentType": ctype, "count": count, "type": type_}
        if mn is not None:
            a["min"] = mn
        if mx is not None:
            a["max"] = mx
        accessors.append(a)
        return len(accessors) - 1

    xs, ys, zs = zip(*positions)
    ai_pos = push_acc(
        push_view(pos_off, len(positions) * 12, 12),
        len(positions),
        "VEC3",
        5126,
        [min(xs), min(ys), min(zs)],
        [max(xs), max(ys), max(zs)],
    )
    ai_nrm = push_acc(push_view(nrm_off, len(normals) * 12, 12), len(normals), "VEC3", 5126)
    ai_uv = push_acc(push_view(uv_off, len(uvs) * 8, 8), len(uvs), "VEC2", 5126)
    ai_j = push_acc(push_view(j_off, len(joints0) * 4, 4), len(joints0), "VEC4", 5121)
    ai_w = push_acc(push_view(w_off, len(weights0) * 16, 16), len(weights0), "VEC4", 5126)
    ai_i = push_acc(push_view(idx_off, len(indices) * 2), len(indices), "SCALAR", 5123)
    ai_ibm = push_acc(push_view(ibm_off, len(ibm) * 64, 64), len(ibm), "MAT4", 5126)

    nodes = []
    bone_node_index = {}
    for name in bone_order:
        local = bones[name].get("origin") or [0, 0, 0]
        nodes.append(
            {
                "name": name,
                "translation": [
                    local[0] * BB_TO_GLTF,
                    local[1] * BB_TO_GLTF,
                    local[2] * BB_TO_GLTF,
                ],
            }
        )
        bone_node_index[name] = len(nodes) - 1
    for name, kids in hierarchy.items():
        if name is None or name not in bone_node_index:
            continue
        child_idxs = [bone_node_index[k] for k in kids if k in bone_node_index]
        if child_idxs:
            nodes[bone_node_index[name]]["children"] = child_idxs

    mesh_node = len(nodes)
    nodes.append({"name": "PlayerMesh", "mesh": 0, "skin": 0})
    root_node = len(nodes)
    nodes.append({"name": "PlayerArmature", "children": [bone_node_index["Hips"], mesh_node]})

    materials = [
        {
            "name": "player",
            "pbrMetallicRoughness": {
                "baseColorFactor": [0.92, 0.78, 0.65, 1.0],
                "metallicFactor": 0,
                "roughnessFactor": 1,
            },
            "alphaMode": "OPAQUE",
        }
    ]
    images = []
    textures = []
    if png_path and png_path.exists():
        png_bytes = png_path.read_bytes()
        images = [
            {
                "uri": "data:image/png;base64,"
                + base64.b64encode(png_bytes).decode("ascii")
            }
        ]
        textures = [{"source": 0, "sampler": 0}]
        materials[0]["pbrMetallicRoughness"]["baseColorTexture"] = {"index": 0}
        materials[0]["pbrMetallicRoughness"].pop("baseColorFactor", None)

    g = {
        "asset": {"version": "2.0", "generator": "export_player_v2_bbmodel_gltf.py"},
        "buffers": [
            {
                "byteLength": len(blob),
                "uri": "data:application/octet-stream;base64,"
                + base64.b64encode(bytes(blob)).decode("ascii"),
            }
        ],
        "bufferViews": views,
        "accessors": accessors,
        "meshes": [
            {
                "primitives": [
                    {
                        "attributes": {
                            "POSITION": ai_pos,
                            "NORMAL": ai_nrm,
                            "TEXCOORD_0": ai_uv,
                            "JOINTS_0": ai_j,
                            "WEIGHTS_0": ai_w,
                        },
                        "indices": ai_i,
                        "material": 0,
                    }
                ]
            }
        ],
        "materials": materials,
        "skins": [
            {
                "name": "PlayerSkin",
                "joints": [bone_node_index[n] for n in bone_order],
                "skeleton": bone_node_index["Hips"],
                "inverseBindMatrices": ai_ibm,
            }
        ],
        "nodes": nodes,
        "scenes": [{"nodes": [root_node]}],
        "scene": 0,
    }
    if images:
        g["images"] = images
        g["textures"] = textures
        g["samplers"] = [{}]

    out_path.write_text(json.dumps(g, separators=(",", ":")), encoding="utf-8")
    print(
        f"Wrote {out_path} verts={len(positions)} tris={len(indices)//3} "
        f"joints={len(bone_order)} fingers="
        f"{sum(1 for n in bone_order if any(x in n for x in ('Thumb','Index','Middle','Ring','Pinky')))}"
    )
    print("  joints:", ", ".join(bone_order))


def main() -> None:
    bb = SRC_BB if SRC_BB.exists() else SRC_BB_FALLBACK
    png = SRC_PNG if SRC_PNG.exists() else (SRC_PNG_FALLBACK if SRC_PNG_FALLBACK.exists() else None)
    if not bb.exists():
        raise SystemExit(f"missing bbmodel: {SRC_BB}")
    export(bb, png, DST_GLTF)
    DST_GLTF_DOCS.write_bytes(DST_GLTF.read_bytes())
    print("Wrote", DST_GLTF_DOCS)


if __name__ == "__main__":
    main()
