"""Rebuild Player_V2: bake finger rotations, stitch hands into body, armature-rig.

Reads the latest Documents Player_V2.bbmodel (user-edited), produces:
  - tools/art/player/Player_V2_rigged.bbmodel
  - c:\\Users\\johnr\\Documents\\Player_V2_rigged.bbmodel
  - tools/art/player/Player_V2_rigged.gltf (skinned, for bake_player_v2_gltf.py)
"""
from __future__ import annotations

import base64
import json
import math
import struct
import uuid
from copy import deepcopy
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SRC = Path(r"c:\Users\johnr\Documents\Player_V2.bbmodel")
DST_ART = REPO / "tools/art/player/Player_V2_rigged.bbmodel"
DST_DOCS = Path(r"c:\Users\johnr\Documents\Player_V2_rigged.bbmodel")
DST_GLTF = REPO / "tools/art/player/Player_V2_rigged.gltf"
DST_GLTF_DOCS = Path(r"c:\Users\johnr\Documents\Player_V2_rigged.gltf")

WELD_EPS = 0.12  # BB units — stitch finger roots to body palm


def new_uuid() -> str:
    return str(uuid.uuid4())


def short_id(used: set[str]) -> str:
    while True:
        s = uuid.uuid4().hex[:4]
        if s not in used:
            used.add(s)
            return s


def rot_matrix(rx, ry, rz):
    ax, ay, az = map(math.radians, (rx, ry, rz))
    cx, sx = math.cos(ax), math.sin(ax)
    cy, sy = math.cos(ay), math.sin(ay)
    cz, sz = math.cos(az), math.sin(az)
    return [
        [cy * cz, cz * sx * sy - cx * sz, sx * sz + cx * cz * sy],
        [cy * sz, cx * cz + sx * sy * sz, cx * sy * sz - cz * sx],
        [-sy, cy * sx, cx * cy],
    ]


def mul(m, v):
    return [
        m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2],
        m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2],
        m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2],
    ]


def world_point(origin, rotation, local):
    return [
        a + b
        for a, b in zip(
            mul(rot_matrix(*(rotation or [0, 0, 0])), local),
            origin or [0, 0, 0],
        )
    ]


def dist(a, b):
    return math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(3)))


def bake_mesh_to_world_origin(el: dict, new_origin=None) -> dict:
    """Return a copy of mesh with verts in world space relative to new_origin, rot=0."""
    out = deepcopy(el)
    o = el.get("origin") or [0, 0, 0]
    rot = el.get("rotation") or [0, 0, 0]
    target = list(new_origin) if new_origin is not None else [0.0, 0.0, 0.0]
    new_verts = {}
    for vid, local in el["vertices"].items():
        w = world_point(o, rot, local)
        new_verts[vid] = [w[0] - target[0], w[1] - target[1], w[2] - target[2]]
    out["vertices"] = new_verts
    out["origin"] = target
    out["rotation"] = [0, 0, 0]
    return out


def cube_to_mesh(cube: dict, name: str) -> dict:
    fr, to = cube["from"], cube["to"]
    origin = [float(c) for c in (cube.get("origin") or [0, 0, 0])]
    x0, y0, z0 = fr
    x1, y1, z1 = to
    used: set[str] = set()
    corners_world = {
        "a": [x0, y0, z0],
        "b": [x1, y0, z0],
        "c": [x1, y1, z0],
        "d": [x0, y1, z0],
        "e": [x0, y0, z1],
        "f": [x1, y0, z1],
        "g": [x1, y1, z1],
        "h": [x0, y1, z1],
    }
    vids = {k: short_id(used) for k in corners_world}
    vertices = {
        vids[k]: [corners_world[k][i] - origin[i] for i in range(3)] for k in corners_world
    }
    faces_def = [
        ("a", "b", "c", "d"),
        ("e", "h", "g", "f"),
        ("a", "e", "f", "b"),
        ("d", "c", "g", "h"),
        ("a", "d", "h", "e"),
        ("b", "f", "g", "c"),
    ]
    tex = 0
    for face in (cube.get("faces") or {}).values():
        if isinstance(face, dict) and face.get("texture") is not None:
            tex = face["texture"]
            break
    faces = {}
    for quad in faces_def:
        ids = [vids[q] for q in quad]
        faces[short_id(used)] = {
            "vertices": ids,
            "uv": {ids[0]: [0, 0], ids[1]: [1, 0], ids[2]: [1, 1], ids[3]: [0, 1]},
            "texture": tex,
        }
    return {
        "name": name,
        "uuid": new_uuid(),
        "type": "mesh",
        "color": cube.get("color", 0),
        "origin": origin,
        "rotation": [0, 0, 0],
        "shading": "flat",
        "export": True,
        "visibility": True,
        "locked": False,
        "render_order": "default",
        "scope": 0,
        "allow_mirror_modeling": True,
        "vertices": vertices,
        "faces": faces,
    }


def stitch_meshes_into_body(body: dict, parts: list[dict]) -> dict:
    """Append part meshes into body (same origin), weld near-duplicate verts."""
    body = deepcopy(body)
    bo = body.get("origin") or [0, 0, 0]
    used = set(body["vertices"].keys()) | set(body.get("faces", {}).keys())
    # Also reserve face uv keys aren't needed in used set beyond vert/face ids

    # Index body world verts for welding
    body_world = {
        vid: [body["vertices"][vid][i] + bo[i] for i in range(3)] for vid in body["vertices"]
    }

    def find_weld(w):
        for vid, bw in body_world.items():
            if dist(w, bw) <= WELD_EPS:
                return vid
        return None

    welded = appended = 0
    for part in parts:
        po = part.get("origin") or [0, 0, 0]
        remap = {}
        for vid, local in part["vertices"].items():
            w = [local[i] + po[i] for i in range(3)]
            hit = find_weld(w)
            if hit is not None:
                remap[vid] = hit
                welded += 1
            else:
                nid = short_id(used)
                body["vertices"][nid] = [w[0] - bo[0], w[1] - bo[1], w[2] - bo[2]]
                body_world[nid] = w
                remap[vid] = nid
                appended += 1
        for face in (part.get("faces") or {}).values():
            old_ids = face["vertices"]
            new_ids = [remap[v] for v in old_ids]
            fid = short_id(used)
            uv_src = face.get("uv") or {}
            uv = {}
            for old_v, new_v in zip(old_ids, new_ids):
                if old_v in uv_src:
                    uv[new_v] = uv_src[old_v]
                else:
                    uv[new_v] = [0, 0]
            body["faces"][fid] = {
                "vertices": new_ids,
                "uv": uv,
                "texture": face.get("texture", 0),
            }
    print(f"  stitch: welded={welded} new_verts={appended} body_verts={len(body['vertices'])}")
    return body


def make_bone(name: str, local_origin, length: float, color: int = 0) -> dict:
    return {
        "type": "armature_bone",
        "name": name,
        "uuid": new_uuid(),
        "export": True,
        "locked": False,
        "visibility": True,
        "origin": [float(local_origin[0]), float(local_origin[1]), float(local_origin[2])],
        "rotation": [0, 0, 0],
        "length": float(length),
        "width": 1.5,
        "connected": True,
        "color": color,
        "vertex_weights": {},
        "isOpen": True,
        "children": [],
    }


def add_weight(bone: dict, mesh_uuid: str, vkey: str, w: float) -> None:
    if w <= 0:
        return
    key = f"{mesh_uuid[:6]}:{vkey}"
    bone["vertex_weights"][key] = bone["vertex_weights"].get(key, 0.0) + w


def smoothstep(edge0, edge1, x):
    if edge0 == edge1:
        return 0.0 if x < edge0 else 1.0
    t = max(0.0, min(1.0, (x - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)


def classify_body_weights(wx, wy, wz) -> dict[str, float]:
    ax = abs(wx)
    blend = 1.2
    if ax > 5.0 and wy >= 24.0:
        side = "Left" if wx >= 0 else "Right"
        shoulder, elbow, wrist = 6.0, 14.5, 19.5
        if ax < shoulder + blend:
            t = smoothstep(shoulder - blend, shoulder + blend, ax)
            return {"Chest": 1.0 - t, f"{side}UpperArm": t}
        if ax < elbow + blend:
            t = smoothstep(elbow - blend, elbow + blend, ax)
            if ax < elbow - blend:
                return {f"{side}UpperArm": 1.0}
            return {f"{side}UpperArm": 1.0 - t, f"{side}LowerArm": t}
        t = smoothstep(wrist - blend, wrist + blend, ax)
        if ax < wrist - blend:
            return {f"{side}LowerArm": 1.0}
        return {f"{side}LowerArm": 1.0 - t, f"{side}Hand": t}
    if ax < 6.5 and wy < 16.5:
        side = "Left" if wx >= 0 else "Right"
        hip, knee, ankle = 15.5, 9.0, 3.5
        if wy > hip - blend:
            t = smoothstep(hip - blend, hip + 0.5, wy)
            return {"Hips": t, f"{side}UpperLeg": 1.0 - t}
        if wy > knee - blend:
            t = smoothstep(knee - blend, knee + blend, wy)
            if wy > knee + blend:
                return {f"{side}UpperLeg": 1.0}
            return {f"{side}UpperLeg": t, f"{side}LowerLeg": 1.0 - t}
        t = smoothstep(ankle - blend, ankle + blend, wy)
        if wy > ankle + blend:
            return {f"{side}LowerLeg": 1.0}
        return {f"{side}LowerLeg": t, f"{side}Foot": 1.0 - t}
    hips_y, spine_y, chest_y = 16.0, 20.0, 26.0
    if wy < hips_y + blend:
        t = smoothstep(hips_y - blend, hips_y + blend, wy)
        if wy < hips_y - blend:
            return {"Hips": 1.0}
        return {"Hips": 1.0 - t, "Spine": t}
    if wy < spine_y + blend:
        t = smoothstep(spine_y - blend, spine_y + blend, wy)
        if wy < spine_y - blend:
            return {"Spine": 1.0}
        return {"Spine": 1.0 - t, "Chest": t}
    if wy < chest_y + blend:
        t = smoothstep(chest_y - blend, chest_y + blend, wy)
        if wy < chest_y - blend:
            return {"Chest": 1.0}
        return {"Chest": 1.0 - t * 0.35, "Neck": t * 0.35} if t > 0 else {"Chest": 1.0}
    return {"Chest": 0.25, "Neck": 0.75}


def assign_full_mesh(bone, mesh):
    for vkey in mesh["vertices"]:
        add_weight(bone, mesh["uuid"], vkey, 1.0)


def outliner_node(uid, children=None):
    return {"uuid": uid, "isOpen": True, "children": children or []}


# --- glTF export helpers ---

BB_TO_GLTF = 1.0 / 8.0  # Blockbench free units → meters-ish; bake will re-normalize


def export_skinned_gltf(meshes: list[dict], bones: dict, bone_order: list[str], hierarchy: dict, path: Path):
    """Minimal glTF 2.0 with one skinned mesh + joints (no animation)."""
    # Merge all meshes into one buffer in world space
    positions = []
    normals = []
    uvs = []
    joints0 = []
    weights0 = []
    indices = []

    bone_index = {name: i for i, name in enumerate(bone_order)}

    # Collect weight maps: mesh_uuid[:6]:vkey -> {bone: w}
    weight_maps = {name: bone["vertex_weights"] for name, bone in bones.items()}

    def weights_for(mesh_uuid, vkey):
        key = f"{mesh_uuid[:6]}:{vkey}"
        wmap = {}
        for bname, vw in weight_maps.items():
            if key in vw:
                wmap[bname] = vw[key]
        if not wmap:
            wmap = {"Hips": 1.0}
        total = sum(wmap.values()) or 1.0
        items = sorted(((n, w / total) for n, w in wmap.items()), key=lambda t: -t[1])[:4]
        while len(items) < 4:
            items.append(("Hips", 0.0))
        # renormalize top4
        s = sum(w for _, w in items) or 1.0
        items = [(n, w / s) for n, w in items]
        return items

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
            # triangulate fan
            for i in range(1, len(ids) - 1):
                indices.extend(
                    [
                        base + vindex[ids[0]],
                        base + vindex[ids[i]],
                        base + vindex[ids[i + 1]],
                    ]
                )
            # pull UVs if present
            uv = face.get("uv") or {}
            for vid in ids:
                if vid in uv and vid in vindex:
                    u, v = uv[vid][:2]
                    # Blockbench UV often 0-16 or 0-1; normalize softly
                    if u > 1.5 or v > 1.5:
                        u, v = u / 64.0, v / 64.0
                    uvs[base + vindex[vid]] = [float(u), float(v)]

    # recompute flat normals
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

    # Joint world matrices for IBM
    parent_of = {}
    for parent, kids in hierarchy.items():
        for kid in kids:
            parent_of[kid] = parent

    world_pos = {}

    def bone_world(name):
        if name in world_pos:
            return world_pos[name]
        parent = parent_of.get(name)
        local = bones[name]["origin"]
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

    # Pack buffers
    blob = bytearray()

    def align4():
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
        return off, len(values)

    def add_u16(values):
        align4()
        off = len(blob)
        for v in values:
            blob.extend(struct.pack("<H", int(v)))
        return off, len(values)

    def add_u8(values, comps):
        align4()
        off = len(blob)
        for v in values:
            blob.extend(struct.pack("<" + "B" * comps, *[int(x) for x in v]))
        return off, len(values)

    pos_off, _ = add_f32(positions, 3)
    nrm_off, _ = add_f32(normals, 3)
    uv_off, _ = add_f32(uvs, 2)
    j_off, _ = add_u8(joints0, 4)
    w_off, _ = add_f32(weights0, 4)
    idx_off, _ = add_u16(indices)

    # IBM: inv(translate(world))
    ibm = []
    for name in bone_order:
        wp = world_pos[name]
        # column-major identity with translation -wp
        ibm.append([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -wp[0], -wp[1], -wp[2], 1])
    ibm_off, _ = add_f32(ibm, 16)

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
    vi = push_view(pos_off, len(positions) * 12, 12)
    ai_pos = push_acc(vi, len(positions), "VEC3", 5126, [min(xs), min(ys), min(zs)], [max(xs), max(ys), max(zs)])
    vi = push_view(nrm_off, len(normals) * 12, 12)
    ai_nrm = push_acc(vi, len(normals), "VEC3", 5126)
    vi = push_view(uv_off, len(uvs) * 8, 8)
    ai_uv = push_acc(vi, len(uvs), "VEC2", 5126)
    vi = push_view(j_off, len(joints0) * 4, 4)
    ai_j = push_acc(vi, len(joints0), "VEC4", 5121)
    vi = push_view(w_off, len(weights0) * 16, 16)
    ai_w = push_acc(vi, len(weights0), "VEC4", 5126)
    vi = push_view(idx_off, len(indices) * 2)
    ai_i = push_acc(vi, len(indices), "SCALAR", 5123)
    vi = push_view(ibm_off, len(ibm) * 64, 64)
    ai_ibm = push_acc(vi, len(ibm), "MAT4", 5126)

    # Nodes: bones + skinned mesh node
    nodes = []
    bone_node_index = {}
    for name in bone_order:
        local = bones[name]["origin"]
        nodes.append(
            {
                "name": name,
                "translation": [
                    local[0] * BB_TO_GLTF,
                    local[1] * BB_TO_GLTF,
                    local[2] * BB_TO_GLTF,
                ],
                "children": [],
            }
        )
        bone_node_index[name] = len(nodes) - 1
    for name, kids in hierarchy.items():
        if name is None:
            continue
        if name in bone_node_index:
            nodes[bone_node_index[name]]["children"] = [bone_node_index[k] for k in kids]

    mesh_node = len(nodes)
    nodes.append({"name": "PlayerMesh", "mesh": 0, "skin": 0})
    root_node = len(nodes)
    nodes.append(
        {
            "name": "PlayerArmature",
            "children": [bone_node_index["Hips"], mesh_node],
        }
    )

    # clear empty children arrays
    for n in nodes:
        if "children" in n and not n["children"]:
            del n["children"]

    g = {
        "asset": {"version": "2.0", "generator": "rebuild_player_v2_rigged.py"},
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
        "materials": [
            {
                "name": "player",
                "pbrMetallicRoughness": {
                    "baseColorFactor": [0.92, 0.78, 0.65, 1.0],
                    "metallicFactor": 0,
                    "roughnessFactor": 1,
                },
                "alphaMode": "OPAQUE",
            }
        ],
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
    path.write_text(json.dumps(g, separators=(",", ":")), encoding="utf-8")
    print("Wrote", path, f"verts={len(positions)} tris={len(indices)//3}")


def main():
    print("Loading", SRC)
    data = json.loads(SRC.read_text(encoding="utf-8"))
    els = data["elements"]
    assert len(els) >= 18

    head = bake_mesh_to_world_origin(els[0], [0, 33, 0])
    head["name"] = "HeadMesh"
    neck = bake_mesh_to_world_origin(els[1], [0, 29.3, 0.6])
    neck["name"] = "NeckMesh"
    body = bake_mesh_to_world_origin(els[4], [0, 24.4, 0.7])
    body["name"] = "BodyMesh"

    brow_r = cube_to_mesh(els[2], "BrowRight")
    brow_l = cube_to_mesh(els[3], "BrowLeft")
    nose = cube_to_mesh(els[5], "Nose")
    # bake cubes to same space (already local-to-origin)
    brow_r = bake_mesh_to_world_origin(brow_r, brow_r["origin"])
    brow_l = bake_mesh_to_world_origin(brow_l, brow_l["origin"])
    nose = bake_mesh_to_world_origin(nose, nose["origin"])

    # Finger indices from prior mapping (left/right). Bake rotations into verts.
    # From inspect: 6-17 are fingers with various origins/rots.
    finger_els = [bake_mesh_to_world_origin(els[i]) for i in range(6, 18)]
    # Classify by world X after bake
    left_fingers, right_fingers = [], []
    for i, fe in enumerate(finger_els):
        o = fe["origin"]
        xs = [fe["vertices"][v][0] + o[0] for v in fe["vertices"]]
        cx = sum(xs) / len(xs)
        fe["name"] = f"{'Left' if cx >= 0 else 'Right'}Finger{len(left_fingers)+1 if cx>=0 else len(right_fingers)+1}"
        if cx >= 0:
            left_fingers.append(fe)
        else:
            right_fingers.append(fe)
    # Rename cleanly
    for i, fe in enumerate(left_fingers, 1):
        fe["name"] = f"LeftFinger{i}"
        fe["uuid"] = new_uuid()
    for i, fe in enumerate(right_fingers, 1):
        fe["name"] = f"RightFinger{i}"
        fe["uuid"] = new_uuid()

    print(f"fingers L={len(left_fingers)} R={len(right_fingers)}")
    print("Stitching fingers into BodyMesh...")
    body["uuid"] = new_uuid()
    body = stitch_meshes_into_body(body, left_fingers + right_fingers)

    # Fresh UUIDs for remaining meshes
    for m in (head, neck, brow_r, brow_l, nose):
        m["uuid"] = new_uuid()
        m["locked"] = False
        m["visibility"] = True

    # Bone world pivots
    BW = {
        "Hips": [0.0, 16.0, 0.5],
        "Spine": [0.0, 20.0, 0.5],
        "Chest": [0.0, 27.0, 0.5],
        "Neck": [0.0, 32.3, 0.6],
        "Head": [0.0, 33.0, 0.0],
        "LeftUpperArm": [5.0, 29.5, 0.5],
        "LeftLowerArm": [14.5, 29.5, 1.0],
        "LeftHand": [20.0, 29.5, 1.2],
        "RightUpperArm": [-5.0, 29.5, 0.5],
        "RightLowerArm": [-14.5, 29.5, 1.0],
        "RightHand": [-20.0, 29.5, 1.2],
        "LeftUpperLeg": [2.2, 15.5, 0.5],
        "LeftLowerLeg": [2.2, 9.0, 0.5],
        "LeftFoot": [2.2, 2.5, 0.5],
        "RightUpperLeg": [-2.2, 15.5, 0.5],
        "RightLowerLeg": [-2.2, 9.0, 0.5],
        "RightFoot": [-2.2, 2.5, 0.5],
    }

    def local(child, parent):
        if parent is None:
            return list(BW[child])
        return [BW[child][i] - BW[parent][i] for i in range(3)]

    def blen(name, child):
        if not child:
            return 4.0
        d = [BW[child][i] - BW[name][i] for i in range(3)]
        return max(2.0, math.sqrt(sum(x * x for x in d)))

    bones = {
        "Hips": make_bone("Hips", local("Hips", None), blen("Hips", "Spine"), 0),
        "Spine": make_bone("Spine", local("Spine", "Hips"), blen("Spine", "Chest"), 0),
        "Chest": make_bone("Chest", local("Chest", "Spine"), blen("Chest", "Neck"), 0),
        "Neck": make_bone("Neck", local("Neck", "Chest"), blen("Neck", "Head"), 5),
        "Head": make_bone("Head", local("Head", "Neck"), 4.0, 5),
        "LeftUpperArm": make_bone("LeftUpperArm", local("LeftUpperArm", "Chest"), blen("LeftUpperArm", "LeftLowerArm"), 1),
        "LeftLowerArm": make_bone("LeftLowerArm", local("LeftLowerArm", "LeftUpperArm"), blen("LeftLowerArm", "LeftHand"), 1),
        "LeftHand": make_bone("LeftHand", local("LeftHand", "LeftLowerArm"), 3.0, 1),
        "RightUpperArm": make_bone("RightUpperArm", local("RightUpperArm", "Chest"), blen("RightUpperArm", "RightLowerArm"), 2),
        "RightLowerArm": make_bone("RightLowerArm", local("RightLowerArm", "RightUpperArm"), blen("RightLowerArm", "RightHand"), 2),
        "RightHand": make_bone("RightHand", local("RightHand", "RightLowerArm"), 3.0, 2),
        "LeftUpperLeg": make_bone("LeftUpperLeg", local("LeftUpperLeg", "Hips"), blen("LeftUpperLeg", "LeftLowerLeg"), 3),
        "LeftLowerLeg": make_bone("LeftLowerLeg", local("LeftLowerLeg", "LeftUpperLeg"), blen("LeftLowerLeg", "LeftFoot"), 3),
        "LeftFoot": make_bone("LeftFoot", local("LeftFoot", "LeftLowerLeg"), 3.0, 3),
        "RightUpperLeg": make_bone("RightUpperLeg", local("RightUpperLeg", "Hips"), blen("RightUpperLeg", "RightLowerLeg"), 4),
        "RightLowerLeg": make_bone("RightLowerLeg", local("RightLowerLeg", "RightUpperLeg"), blen("RightLowerLeg", "RightFoot"), 4),
        "RightFoot": make_bone("RightFoot", local("RightFoot", "RightLowerLeg"), 3.0, 4),
    }

    assign_full_mesh(bones["Head"], head)
    assign_full_mesh(bones["Head"], brow_l)
    assign_full_mesh(bones["Head"], brow_r)
    assign_full_mesh(bones["Head"], nose)
    assign_full_mesh(bones["Neck"], neck)

    print("Painting body (+stitched hands) weights...")
    bo = body.get("origin") or [0, 0, 0]
    for vkey, local_v in body["vertices"].items():
        wx, wy, wz = local_v[0] + bo[0], local_v[1] + bo[1], local_v[2] + bo[2]
        # Stitched fingers past wrist → full Hand
        if abs(wx) > 19.2 and wy > 26.0:
            side = "Left" if wx >= 0 else "Right"
            add_weight(bones[f"{side}Hand"], body["uuid"], vkey, 1.0)
            continue
        wmap = classify_body_weights(wx, wy, wz)
        total = sum(wmap.values()) or 1.0
        for bname, w in wmap.items():
            nw = w / total
            if nw > 0.001:
                add_weight(bones[bname], body["uuid"], vkey, nw)

    def link(parent, *kids):
        bones[parent]["children"] = [bones[k]["uuid"] for k in kids]

    link("Hips", "Spine", "LeftUpperLeg", "RightUpperLeg")
    link("Spine", "Chest")
    link("Chest", "Neck", "LeftUpperArm", "RightUpperArm")
    link("Neck", "Head")
    link("LeftUpperArm", "LeftLowerArm")
    link("LeftLowerArm", "LeftHand")
    link("RightUpperArm", "RightLowerArm")
    link("RightLowerArm", "RightHand")
    link("LeftUpperLeg", "LeftLowerLeg")
    link("LeftLowerLeg", "LeftFoot")
    link("RightUpperLeg", "RightLowerLeg")
    link("RightLowerLeg", "RightFoot")

    all_meshes = [head, neck, body, brow_r, brow_l, nose]
    bone_list = list(bones.values())
    mesh_uuids = [m["uuid"] for m in all_meshes]
    armature = {
        "type": "armature",
        "name": "PlayerArmature",
        "uuid": new_uuid(),
        "export": True,
        "locked": False,
        "visibility": True,
        "isOpen": True,
        "children": [bones["Hips"]["uuid"]] + mesh_uuids,
    }

    def bone_tree(name):
        kids = []
        for cu in bones[name]["children"]:
            cn = next(b["name"] for b in bone_list if b["uuid"] == cu)
            kids.append(bone_tree(cn))
        return outliner_node(bones[name]["uuid"], kids)

    data["elements"] = all_meshes + [armature] + bone_list
    data["groups"] = []
    data["outliner"] = [outliner_node(armature["uuid"], mesh_uuids + [bone_tree("Hips")])]
    data["name"] = "Player_V2_rigged"

    payload = json.dumps(data, separators=(",", ":"))
    DST_ART.parent.mkdir(parents=True, exist_ok=True)
    DST_ART.write_text(payload, encoding="utf-8")
    DST_DOCS.write_text(payload, encoding="utf-8")
    print("Wrote", DST_ART)
    print("Wrote", DST_DOCS)

    bone_order = [
        "Hips",
        "Spine",
        "Chest",
        "Neck",
        "Head",
        "LeftUpperArm",
        "LeftLowerArm",
        "LeftHand",
        "RightUpperArm",
        "RightLowerArm",
        "RightHand",
        "LeftUpperLeg",
        "LeftLowerLeg",
        "LeftFoot",
        "RightUpperLeg",
        "RightLowerLeg",
        "RightFoot",
    ]
    hierarchy = {
        "Hips": ["Spine", "LeftUpperLeg", "RightUpperLeg"],
        "Spine": ["Chest"],
        "Chest": ["Neck", "LeftUpperArm", "RightUpperArm"],
        "Neck": ["Head"],
        "LeftUpperArm": ["LeftLowerArm"],
        "LeftLowerArm": ["LeftHand"],
        "RightUpperArm": ["RightLowerArm"],
        "RightLowerArm": ["RightHand"],
        "LeftUpperLeg": ["LeftLowerLeg"],
        "LeftLowerLeg": ["LeftFoot"],
        "RightUpperLeg": ["RightLowerLeg"],
        "RightLowerLeg": ["RightFoot"],
        "Head": [],
        "LeftHand": [],
        "RightHand": [],
        "LeftFoot": [],
        "RightFoot": [],
    }
    # Bone origins in make_bone are LOCAL; BW is world — fix glTF exporter to use BW for IBM
    # Override bones[*].origin temporarily? export uses bones[name]['origin'] as LOCAL.
    # For IBM we compute from hierarchy of locals — good.
    # But make_bone stored LOCAL already. Good.

    export_skinned_gltf(all_meshes, bones, bone_order, hierarchy, DST_GLTF)
    DST_GLTF_DOCS.write_text(DST_GLTF.read_text(encoding="utf-8"), encoding="utf-8")
    print("Wrote", DST_GLTF_DOCS)


if __name__ == "__main__":
    main()
