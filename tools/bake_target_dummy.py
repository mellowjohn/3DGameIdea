"""Bake Target_Dummy.bbmodel into a skinned combat dummy (Idle + HitReact)."""

from __future__ import annotations

import base64
import io
import json
import math
import struct
import sys
from pathlib import Path

from bake_tree_bbmodel import REPO, euler_xyz_deg_to_mat, pad4, sample_uv, transform_point, triangulate
from PIL import Image

SRC = REPO / "tools/art/target-dummy/Target_Dummy.bbmodel"
DST = REPO / "samples/open-world-rpg/assets/models/target_dummy.gltf"
PNG = REPO / "samples/open-world-rpg/assets/models/target_dummy.png"
ART_PNG = REPO / "tools/art/target-dummy/TargetDummy.png"
TARGET_HEIGHT = 2.2
GENERATOR = "AI RPG Engine target_dummy skinned bake from Target_Dummy.bbmodel v2-hitreact"


def euler_deg_to_quat(x: float, y: float, z: float) -> tuple[float, float, float, float]:
    rx, ry, rz = (math.radians(x), math.radians(y), math.radians(z))
    cx, sx = math.cos(rx * 0.5), math.sin(rx * 0.5)
    cy, sy = math.cos(ry * 0.5), math.sin(ry * 0.5)
    cz, sz = math.cos(rz * 0.5), math.sin(rz * 0.5)
    qx = sx * cy * cz + cx * sy * sz
    qy = cx * sy * cz - sx * cy * sz
    qz = cx * cy * sz + sx * sy * cz
    qw = cx * cy * cz - sx * sy * sz
    return (qx, qy, qz, qw)


def walk_outliner(nodes: list, parent_uuid: str | None, parent_of: dict[str, str | None]) -> None:
    for node in nodes or []:
        if isinstance(node, str):
            parent_of[node] = parent_uuid
            continue
        uid = node.get("uuid")
        if not uid:
            continue
        parent_of[uid] = parent_uuid
        walk_outliner(node.get("children") or [], uid, parent_of)


def bake() -> None:
    if not SRC.exists():
        raise SystemExit(f"missing source {SRC}")
    model = json.loads(SRC.read_text(encoding="utf-8"))
    res = model.get("resolution") or {"width": 128, "height": 128}
    tw_res = float(res["width"])
    th_res = float(res["height"])
    textures = model.get("textures") or []
    if not textures:
        raise RuntimeError("bbmodel has no textures")
    src_uri = textures[0].get("source") or ""
    if not src_uri.startswith("data:image/png;base64,"):
        raise RuntimeError("expected embedded PNG texture source")
    tex = Image.open(io.BytesIO(base64.b64decode(src_uri.split(",", 1)[1]))).convert("RGBA")

    groups = {g["uuid"]: g for g in (model.get("groups") or []) if g.get("uuid")}
    elements = {e["uuid"]: e for e in (model.get("elements") or []) if e.get("uuid")}
    parent_of: dict[str, str | None] = {}
    walk_outliner(model.get("outliner") or [], None, parent_of)

    bone_order: list[str] = []

    def walk_bones(uid: str | None) -> None:
        if uid is None:
            for child, parent in parent_of.items():
                if parent is None and child in groups:
                    walk_bones(child)
            return
        if uid not in groups:
            return
        bone_order.append(uid)
        for child, parent in parent_of.items():
            if parent == uid and child in groups:
                walk_bones(child)

    walk_bones(None)
    if not bone_order:
        raise RuntimeError("bbmodel has no bone groups")
    bone_index = {uid: i for i, uid in enumerate(bone_order)}

    positions: list[tuple[float, float, float]] = []
    colors: list[tuple[float, float, float, float]] = []
    uv_coords: list[tuple[float, float]] = []
    joints0: list[tuple[int, int, int, int]] = []
    weights0: list[tuple[float, float, float, float]] = []
    indices: list[int] = []

    for uid, element in elements.items():
        if element.get("visibility") is False or element.get("export") is False:
            continue
        if element.get("type") != "mesh":
            continue
        parent = parent_of.get(uid)
        joint = bone_index.get(parent or "", 0)
        verts = element.get("vertices") or {}
        faces = element.get("faces") or {}
        origin = element.get("origin") or [0.0, 0.0, 0.0]
        rotation = element.get("rotation") or [0.0, 0.0, 0.0]
        rot = euler_xyz_deg_to_mat(rotation[0], rotation[1], rotation[2])
        for face in faces.values():
            if face.get("texture") is None:
                continue
            vids = face.get("vertices") or []
            if len(vids) < 3:
                continue
            uvs = face.get("uv") or {}
            for a, b, c in triangulate(vids):
                tri = []
                for vid in (a, b, c):
                    local = verts[vid]
                    world = transform_point(rot, local, origin)
                    uv_px = uvs.get(vid) or [0.0, 0.0]
                    u = float(uv_px[0]) / tw_res
                    v = float(uv_px[1]) / th_res
                    positions.append(world)
                    uv_coords.append((u, v))
                    colors.append(sample_uv(tex, u, v))
                    joints0.append((joint, 0, 0, 0))
                    weights0.append((1.0, 0.0, 0.0, 0.0))
                    tri.append(len(positions) - 1)
                indices.extend(tri)

    if not positions:
        raise RuntimeError("no mesh geometry exported")

    xs = [p[0] for p in positions]
    ys = [p[1] for p in positions]
    zs = [p[2] for p in positions]
    minx, maxx = min(xs), max(xs)
    miny, maxy = min(ys), max(ys)
    minz, maxz = min(zs), max(zs)
    cx = 0.5 * (minx + maxx)
    cz = 0.5 * (minz + maxz)
    height = maxy - miny
    scale = TARGET_HEIGHT / height if height > 1e-6 else 1.0

    def xform(p: tuple[float, float, float]) -> tuple[float, float, float]:
        return ((p[0] - cx) * scale, (p[1] - miny) * scale, (p[2] - cz) * scale)

    norm_pos = [xform(p) for p in positions]
    bone_world = []
    for uid in bone_order:
        origin = groups[uid].get("origin") or [0.0, 0.0, 0.0]
        bone_world.append(xform((float(origin[0]), float(origin[1]), float(origin[2]))))

    rel_t: list[tuple[float, float, float]] = []
    children: list[list[int]] = [[] for _ in bone_order]
    for i, uid in enumerate(bone_order):
        parent_uid = parent_of.get(uid)
        if parent_uid and parent_uid in bone_index:
            p = bone_index[parent_uid]
            children[p].append(i)
            pw = bone_world[p]
            cw = bone_world[i]
            rel_t.append((cw[0] - pw[0], cw[1] - pw[1], cw[2] - pw[2]))
        else:
            rel_t.append(bone_world[i])

    ident = euler_deg_to_quat(0.0, 0.0, 0.0)
    ibm: list[float] = []
    for wx, wy, wz in bone_world:
        ibm.extend([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -wx, -wy, -wz, 1])

    hit_keys = []
    for anim in model.get("animations") or []:
        name = str(anim.get("name") or "")
        if "HitReact" not in name:
            continue
        animators = anim.get("animators") or {}
        for uid, channel in animators.items():
            if uid not in bone_index:
                continue
            for kf in channel.get("keyframes") or []:
                if kf.get("channel") != "rotation":
                    continue
                pts = kf.get("data_points") or [{}]
                pt = pts[0] if pts else {}
                hit_keys.append(
                    (
                        float(kf.get("time") or 0.0),
                        bone_index[uid],
                        euler_deg_to_quat(float(pt.get("x") or 0), float(pt.get("y") or 0), float(pt.get("z") or 0)),
                    )
                )
    hit_keys.sort()
    if not hit_keys:
        raise RuntimeError("TargetDummy_HitReact rotation keys missing")

    nverts = len(norm_pos)
    nidx = len(indices)
    pos_bytes = pad4(b"".join(struct.pack("<fff", *p) for p in norm_pos))
    col_bytes = pad4(b"".join(struct.pack("<ffff", *c) for c in colors))
    uv_bytes = pad4(b"".join(struct.pack("<ff", *uv) for uv in uv_coords))
    joint_bytes = pad4(b"".join(struct.pack("<BBBB", *j) for j in joints0))
    weight_bytes = pad4(b"".join(struct.pack("<ffff", *w) for w in weights0))
    idx_bytes = pad4(b"".join(struct.pack("<H", i) for i in indices))
    ibm_bytes = pad4(b"".join(struct.pack("<f", v) for v in ibm))

    # Idle: identity rotation on UpperBody at 0 and 0.05s so duration > 0.
    idle_times = struct.pack("<ff", 0.0, 0.05)
    idle_rots = b"".join(struct.pack("<ffff", *ident) for _ in range(2))
    hit_times_list = sorted({t for t, _, _ in hit_keys})
    # One sampler for the animated joint (UpperBody). Other bones stay bind pose.
    animated_joint = hit_keys[0][1]
    hit_rot_by_t = {t: q for t, j, q in hit_keys if j == animated_joint}
    hit_times = b"".join(struct.pack("<f", t) for t in hit_times_list)
    hit_rots = b"".join(struct.pack("<ffff", *hit_rot_by_t[t]) for t in hit_times_list)

    blob = (
        pos_bytes
        + col_bytes
        + uv_bytes
        + joint_bytes
        + weight_bytes
        + idx_bytes
        + ibm_bytes
        + idle_times
        + idle_rots
        + hit_times
        + hit_rots
    )
    b64 = base64.b64encode(blob).decode("ascii")
    tex.save(PNG, format="PNG")
    ART_PNG.write_bytes(PNG.read_bytes())

    mesh_node = len(bone_order)
    nodes = []
    for i, uid in enumerate(bone_order):
        node = {
            "name": groups[uid].get("name") or f"bone_{i}",
            "translation": list(rel_t[i]),
            "rotation": list(ident),
        }
        if children[i]:
            node["children"] = children[i]
        nodes.append(node)
    nodes.append({"name": "TargetDummy", "mesh": 0, "skin": 0})

    nys = [p[1] for p in norm_pos]
    nxs = [p[0] for p in norm_pos]
    nzs = [p[2] for p in norm_pos]
    views = []
    accessors = []
    packed = [
        (pos_bytes, 34962),
        (col_bytes, 34962),
        (uv_bytes, 34962),
        (joint_bytes, 34962),
        (weight_bytes, 34962),
        (idx_bytes, 34963),
        (ibm_bytes, None),
        (idle_times, None),
        (idle_rots, None),
        (hit_times, None),
        (hit_rots, None),
    ]
    cursor = 0
    for chunk, target in packed:
        view = {"buffer": 0, "byteOffset": cursor, "byteLength": len(chunk)}
        if target is not None:
            view["target"] = target
        views.append(view)
        cursor += len(chunk)

    accessors = [
        {
            "bufferView": 0,
            "componentType": 5126,
            "count": nverts,
            "type": "VEC3",
            "min": [min(nxs), min(nys), min(nzs)],
            "max": [max(nxs), max(nys), max(nzs)],
        },
        {"bufferView": 1, "componentType": 5126, "count": nverts, "type": "VEC4"},
        {"bufferView": 2, "componentType": 5126, "count": nverts, "type": "VEC2"},
        {"bufferView": 3, "componentType": 5121, "count": nverts, "type": "VEC4"},
        {"bufferView": 4, "componentType": 5126, "count": nverts, "type": "VEC4"},
        {"bufferView": 5, "componentType": 5123, "count": nidx, "type": "SCALAR"},
        {"bufferView": 6, "componentType": 5126, "count": len(bone_order), "type": "MAT4"},
        {"bufferView": 7, "componentType": 5126, "count": 2, "type": "SCALAR", "min": [0.0], "max": [0.05]},
        {"bufferView": 8, "componentType": 5126, "count": 2, "type": "VEC4"},
        {
            "bufferView": 9,
            "componentType": 5126,
            "count": len(hit_times_list),
            "type": "SCALAR",
            "min": [hit_times_list[0]],
            "max": [hit_times_list[-1]],
        },
        {"bufferView": 10, "componentType": 5126, "count": len(hit_times_list), "type": "VEC4"},
    ]

    upper_node = animated_joint
    out = {
        "asset": {"version": "2.0", "generator": GENERATOR},
        "scenes": [{"nodes": [0, mesh_node], "name": "TargetDummy"}],
        "scene": 0,
        "nodes": nodes,
        "meshes": [
            {
                "name": "TargetDummy",
                "primitives": [
                    {
                        "attributes": {
                            "POSITION": 0,
                            "COLOR_0": 1,
                            "TEXCOORD_0": 2,
                            "JOINTS_0": 3,
                            "WEIGHTS_0": 4,
                        },
                        "indices": 5,
                        "material": 0,
                        "mode": 4,
                    }
                ],
            }
        ],
        "skins": [{"name": "TargetDummySkin", "joints": list(range(len(bone_order))), "inverseBindMatrices": 6}],
        "animations": [
            {
                "name": "Idle",
                "samplers": [{"input": 7, "interpolation": "LINEAR", "output": 8}],
                "channels": [{"sampler": 0, "target": {"node": upper_node, "path": "rotation"}}],
            },
            {
                "name": "HitReact",
                "samplers": [{"input": 9, "interpolation": "LINEAR", "output": 10}],
                "channels": [{"sampler": 0, "target": {"node": upper_node, "path": "rotation"}}],
            },
        ],
        "materials": [
            {
                "name": "TargetDummyAtlas",
                "pbrMetallicRoughness": {
                    "baseColorTexture": {"index": 0},
                    "metallicFactor": 0.0,
                    "roughnessFactor": 1.0,
                },
            }
        ],
        "textures": [{"name": "TargetDummyAtlas", "source": 0, "sampler": 0}],
        "images": [{"name": "TargetDummyAtlas", "uri": PNG.name}],
        "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 33071}],
        "accessors": accessors,
        "bufferViews": views,
        "buffers": [{"byteLength": len(blob), "uri": "data:application/octet-stream;base64," + b64}],
    }
    DST.parent.mkdir(parents=True, exist_ok=True)
    DST.write_text(json.dumps(out, indent=2), encoding="utf-8")
    print(
        f"[TargetDummy] skinned joints={len(bone_order)} verts={nverts} "
        f"h={max(nys) - min(nys):.3f} clips=Idle,HitReact"
    )


def main() -> None:
    bake()


if __name__ == "__main__":
    sys.exit(main() or 0)
