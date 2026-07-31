#!/usr/bin/env python3
"""Export GoodPlayerModel.bbmodel → skinned GoodPlayerModel.gltf (hands + Idle/Run/Fall)."""
from __future__ import annotations

import base64
import json
import math
import struct
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SRC_BB_CANDIDATES = [
    Path(r"c:\Users\johnr\Documents\GoodPlayerModel_rigged.bbmodel"),
    Path(r"c:\Users\johnr\Documents\GoodPlayerModel.bbmodel"),
    REPO / "tools/art/player/GoodPlayerModel_rigged.bbmodel",
]
SRC_PNG_CANDIDATES = [
    Path(r"c:\Users\johnr\Documents\Models\GoodPlayerModel.png"),
    Path(r"c:\Users\johnr\Documents\GoodPlayerModel.png"),
    REPO / "tools/art/player/GoodPlayerModel.png",
]
OUT_PATHS = [
    Path(r"c:\Users\johnr\Documents\Models\GoodPlayerModel.gltf"),
    REPO / "tools/art/player/GoodPlayerModel.gltf",
]

# Match Blockbench free-format glTF default scale (export options scale=16).
BB_TO_GLTF = 1.0 / 16.0
UV_SIZE = 128.0


def first_existing(paths: list[Path]) -> Path | None:
    for p in paths:
        if p.exists():
            return p
    return None


def euler_deg_to_quat(x: float, y: float, z: float) -> list[float]:
    """Blockbench XYZ intrinsic degrees → glTF quaternion xyzw."""
    rx, ry, rz = (math.radians(x), math.radians(y), math.radians(z))
    cx, sx = math.cos(rx * 0.5), math.sin(rx * 0.5)
    cy, sy = math.cos(ry * 0.5), math.sin(ry * 0.5)
    cz, sz = math.cos(rz * 0.5), math.sin(rz * 0.5)
    # XYZ
    qx = sx * cy * cz + cx * sy * sz
    qy = cx * sy * cz - sx * cy * sz
    qz = cx * cy * sz + sx * sy * cz
    qw = cx * cy * cz - sx * sy * sz
    return [qx, qy, qz, qw]


def export(bb_path: Path, png_path: Path | None, out_paths: list[Path]) -> None:
    data = json.loads(bb_path.read_text(encoding="utf-8"))
    bones = {
        e["name"]: e
        for e in data["elements"]
        if e.get("type") == "armature_bone" and e.get("name")
    }
    by_uuid = {e["uuid"]: e for e in data["elements"] if e.get("uuid")}
    meshes = [e for e in data["elements"] if e.get("type") == "mesh" and "vertices" in e]
    if not bones or "Hips" not in bones:
        raise SystemExit("bbmodel missing Hips armature bone")

    hierarchy: dict[str | None, list[str]] = {None: ["Hips"]}
    for name, bone in bones.items():
        kids = []
        for cu in bone.get("children") or []:
            child = by_uuid.get(cu)
            if child and child.get("type") == "armature_bone" and child.get("name") in bones:
                kids.append(child["name"])
        hierarchy[name] = kids

    bone_order: list[str] = []

    def walk(n: str) -> None:
        bone_order.append(n)
        for c in hierarchy.get(n) or []:
            walk(c)

    walk("Hips")
    bone_index = {n: i for i, n in enumerate(bone_order)}

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

    mesh_vert_counts = {}
    for mesh in meshes:
        o = mesh.get("origin") or [0, 0, 0]
        rot = mesh.get("rotation") or [0, 0, 0]
        rx, ry, rz = (math.radians(rot[0]), math.radians(rot[1]), math.radians(rot[2]))
        # XYZ rotation matrix
        cx, sx = math.cos(rx), math.sin(rx)
        cy, sy = math.cos(ry), math.sin(ry)
        cz, sz = math.cos(rz), math.sin(rz)
        r00, r01, r02 = cy * cz, -cy * sz, sy
        r10 = sx * sy * cz + cx * sz
        r11 = -sx * sy * sz + cx * cz
        r12 = -sx * cy
        r20 = -cx * sy * cz + sx * sz
        r21 = cx * sy * sz + sx * cz
        r22 = cx * cy

        base = len(positions)
        vkeys = list(mesh["vertices"].keys())
        vindex = {k: i for i, k in enumerate(vkeys)}
        for vkey in vkeys:
            lx, ly, lz = mesh["vertices"][vkey]
            wx = r00 * lx + r01 * ly + r02 * lz + o[0]
            wy = r10 * lx + r11 * ly + r12 * lz + o[1]
            wz = r20 * lx + r21 * ly + r22 * lz + o[2]
            positions.append([wx * BB_TO_GLTF, wy * BB_TO_GLTF, wz * BB_TO_GLTF])
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
                    uvs[base + vindex[vid]] = [float(u) / UV_SIZE, float(v) / UV_SIZE]
        mesh_vert_counts[mesh.get("name", "?")] = len(vkeys)

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

    # Animations from bbmodel
    animations = []
    for anim in data.get("animations") or []:
        name = anim.get("name")
        if not name:
            continue
        channels = []
        samplers = []
        for an in (anim.get("animators") or {}).values():
            bname = an.get("name")
            if bname not in bone_node_index:
                continue
            by_ch: dict[str, list] = {}
            for kf in an.get("keyframes") or []:
                ch = kf.get("channel")
                if ch not in ("rotation", "position"):
                    continue
                dp = (kf.get("data_points") or [{}])[0]
                by_ch.setdefault(ch, []).append(
                    (
                        float(kf.get("time") or 0),
                        float(dp.get("x") or 0),
                        float(dp.get("y") or 0),
                        float(dp.get("z") or 0),
                    )
                )
            for ch, keys in by_ch.items():
                keys = sorted(keys, key=lambda t: t[0])
                times = [k[0] for k in keys]
                if ch == "rotation":
                    outs = [euler_deg_to_quat(k[1], k[2], k[3]) for k in keys]
                    out_type, comps = "VEC4", 4
                    path = "rotation"
                else:
                    outs = [
                        [k[1] * BB_TO_GLTF, k[2] * BB_TO_GLTF, k[3] * BB_TO_GLTF]
                        for k in keys
                    ]
                    out_type, comps = "VEC3", 3
                    path = "translation"
                t_off = add_f32(times, 1)
                o_off = add_f32(outs, comps)
                t_view = push_view(t_off, len(times) * 4)
                o_view = push_view(o_off, len(outs) * 4 * comps, 4 * comps)
                t_acc = push_acc(
                    t_view,
                    len(times),
                    "SCALAR",
                    5126,
                    [min(times)],
                    [max(times)],
                )
                o_acc = push_acc(o_view, len(outs), out_type, 5126)
                si = len(samplers)
                samplers.append(
                    {"input": t_acc, "output": o_acc, "interpolation": "LINEAR"}
                )
                channels.append(
                    {
                        "sampler": si,
                        "target": {"node": bone_node_index[bname], "path": path},
                    }
                )
        if channels:
            animations.append({"name": name, "samplers": samplers, "channels": channels})

    g = {
        "asset": {
            "version": "2.0",
            "generator": "export_goodplayer_bbmodel_gltf.py",
        },
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
        "nodes": nodes,
        "skins": [
            {
                "name": "PlayerArmature",
                "joints": [bone_node_index[n] for n in bone_order],
                "inverseBindMatrices": ai_ibm,
                "skeleton": bone_node_index["Hips"],
            }
        ],
        "materials": materials,
        "scene": 0,
        "scenes": [{"nodes": [len(nodes) - 1]}],
        "animations": animations,
    }
    if images:
        g["images"] = images
        g["textures"] = textures
        g["samplers"] = [{}]

    text = json.dumps(g, separators=(",", ":"))
    for out in out_paths:
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text, encoding="utf-8")
        print("Wrote", out, "bytes", len(text))

    handish = sum(
        c
        for n, c in mesh_vert_counts.items()
        if any(k in n for k in ("Palm", "Thumb", "Index", "Middle", "Ring", "Pinky", "Wrist"))
    )
    print(
        f"meshes={len(meshes)} verts={len(positions)} tris={len(indices)//3} "
        f"handVerts={handish} bones={len(bone_order)} anims={[a['name'] for a in animations]}"
    )
    print(f"pos y[{min(ys):.3f},{max(ys):.3f}] h={max(ys)-min(ys):.3f}")


def main() -> None:
    bb = first_existing(SRC_BB_CANDIDATES)
    if bb is None:
        raise SystemExit(f"missing bbmodel; tried {SRC_BB_CANDIDATES}")
    png = first_existing(SRC_PNG_CANDIDATES)
    print("Source", bb)
    print("Atlas", png)
    export(bb, png, OUT_PATHS)


if __name__ == "__main__":
    main()
