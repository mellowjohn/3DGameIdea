"""Flatten Player_V2 hands for a natural T-pose (palms down).

Rotates hand/finger geometry -90° around the arm (+X / -X) through each wrist
so finger spread lies in Z and the palm faces the ground.
Edits Documents Player_V2_rigged.bbmodel + .gltf, then leaves bake to normalize.
"""
from __future__ import annotations

import base64
import json
import math
import struct
from copy import deepcopy
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
BB = Path(r"c:\Users\johnr\Documents\Player_V2_rigged.bbmodel")
GLTF = Path(r"c:\Users\johnr\Documents\Player_V2_rigged.gltf")
BB_ORIG = Path(r"c:\Users\johnr\Documents\Player_V2.bbmodel")  # unrigged reference optional

# World-space wrist pivots (from player rig / Blockbench bone chain)
LEFT_WRIST = (21.0, 29.5, 1.2)
RIGHT_WRIST = (-21.0, 29.5, 1.2)


def rot_x_neg90(p, pivot):
    """(x,y,z) -> (x, z, -y) about pivot — palms down for +X arm."""
    x, y, z = p[0] - pivot[0], p[1] - pivot[1], p[2] - pivot[2]
    return [x + pivot[0], z + pivot[1], -y + pivot[2]]


def rot_x_pos90(p, pivot):
    """Mirror for -X arm: (x,y,z) -> (x, -z, y) about pivot so palms also face down."""
    x, y, z = p[0] - pivot[0], p[1] - pivot[1], p[2] - pivot[2]
    return [x + pivot[0], -z + pivot[1], y + pivot[2]]


def world_point(origin, rotation, local):
    # Blockbench mesh rotation is degrees XYZ; fingers mostly identity except thumbs.
    rx, ry, rz = [math.radians(a) for a in (rotation or [0, 0, 0])]
    cx, sx = math.cos(rx), math.sin(rx)
    cy, sy = math.cos(ry), math.sin(ry)
    cz, sz = math.cos(rz), math.sin(rz)
    # Rz * Ry * Rx
    m = [
        [cy * cz, cz * sx * sy - cx * sz, sx * sz + cx * cz * sy],
        [cy * sz, cx * cz + sx * sy * sz, cx * sy * sz - cz * sx],
        [-sy, cy * sx, cx * cy],
    ]
    r = [
        m[0][0] * local[0] + m[0][1] * local[1] + m[0][2] * local[2],
        m[1][0] * local[0] + m[1][1] * local[1] + m[1][2] * local[2],
        m[2][0] * local[0] + m[2][1] * local[1] + m[2][2] * local[2],
    ]
    return [r[0] + origin[0], r[1] + origin[1], r[2] + origin[2]]


def flatten_bbmodel() -> dict:
    data = json.loads(BB.read_text(encoding="utf-8"))
    finger_names = {f"LeftFinger{i}" for i in range(1, 7)} | {f"RightFinger{i}" for i in range(1, 7)}

    for e in data["elements"]:
        name = e.get("name", "")
        if name not in finger_names and name != "BodyMesh":
            continue
        o = e.get("origin") or [0, 0, 0]
        rot = e.get("rotation") or [0, 0, 0]
        verts = e.get("vertices") or {}
        if not verts:
            continue

        if name.startswith("Left"):
            pivot, xform = LEFT_WRIST, rot_x_neg90
        elif name.startswith("Right"):
            pivot, xform = RIGHT_WRIST, rot_x_pos90
        else:
            # BodyMesh: transform verts in each hand region separately
            new_verts = {}
            for vid, local in verts.items():
                w = world_point(o, rot, local)
                if w[0] > 16.0 and w[1] > 26.0:
                    w2 = rot_x_neg90(w, LEFT_WRIST)
                elif w[0] < -16.0 and w[1] > 26.0:
                    w2 = rot_x_pos90(w, RIGHT_WRIST)
                else:
                    w2 = w
                # store back as local (ignore mesh rotation — BodyMesh rot is 0)
                new_verts[vid] = [w2[0] - o[0], w2[1] - o[1], w2[2] - o[2]]
            e["vertices"] = new_verts
            print(f"BodyMesh: flattened hand regions")
            continue

        # Finger meshes: bake world flatten into local verts; clear odd thumb Z rot
        # so export uses the new flat placement.
        new_verts = {}
        for vid, local in verts.items():
            w = world_point(o, rot, local)
            w2 = xform(w, pivot)
            new_verts[vid] = [w2[0] - o[0], w2[1] - o[1], w2[2] - o[2]]
        e["vertices"] = new_verts
        e["rotation"] = [0, 0, 0]
        print(f"{name}: flattened about {pivot}")

    return data


def read_gltf_acc(g, raw, idx):
    acc = g["accessors"][idx]
    bv = g["bufferViews"][acc["bufferView"]]
    off = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    comps = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[acc["type"]]
    fmt = {5126: "f", 5123: "H", 5121: "B"}[acc["componentType"]]
    size = {"f": 4, "H": 2, "B": 1}[fmt]
    stride = bv.get("byteStride", size * comps)
    out = []
    for i in range(acc["count"]):
        vals = struct.unpack_from("<" + fmt * comps, raw, off + i * stride)
        out.append(vals if comps > 1 else vals[0])
    return out


def write_gltf_f32_vec3(g, raw, idx, values):
    acc = g["accessors"][idx]
    bv = g["bufferViews"][acc["bufferView"]]
    off = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    stride = bv.get("byteStride", 12)
    for i, v in enumerate(values):
        struct.pack_into("<fff", raw, off + i * stride, float(v[0]), float(v[1]), float(v[2]))
    xs, ys, zs = [v[0] for v in values], [v[1] for v in values], [v[2] for v in values]
    acc["min"] = [min(xs), min(ys), min(zs)]
    acc["max"] = [max(xs), max(ys), max(zs)]


def flatten_gltf():
    """Apply the same wrist rotations in glTF space (Blockbench export units)."""
    g = json.loads(GLTF.read_text(encoding="utf-8"))
    raw = bytearray(base64.b64decode(g["buffers"][0]["uri"].split(",", 1)[1]))
    prim = g["meshes"][0]["primitives"][0]
    pos = [list(p) for p in read_gltf_acc(g, raw, prim["attributes"]["POSITION"])]
    joints = read_gltf_acc(g, raw, prim["attributes"]["JOINTS_0"])
    weights = read_gltf_acc(g, raw, prim["attributes"]["WEIGHTS_0"])
    names = [g["nodes"][j].get("name") for j in g["skins"][0]["joints"]]
    li = names.index("LeftHand")
    ri = names.index("RightHand")
    lla = names.index("LeftLowerArm")
    rla = names.index("RightLowerArm")

    # Scale factor: bb world / gltf (from prior exports ~8.0; calibrate from wrist X)
    # Left wrist bb x=21 → find hand verts median x
    hand_xs = [p[0] for i, p in enumerate(pos) if weights[i][0] > 0.5 and joints[i][0] == li]
    if not hand_xs:
        raise SystemExit("no LeftHand verts in glTF")
    gltf_wrist_x = sorted(hand_xs)[len(hand_xs) // 2]
    scale = abs(LEFT_WRIST[0] / gltf_wrist_x) if abs(gltf_wrist_x) > 1e-6 else 8.0
    print(f"glTF hand scale~{scale:.4f} (bb_wrist_x / gltf_hand_x)")

    def to_bb(p):
        return [p[0] * scale, p[1] * scale, p[2] * scale]

    def to_gltf(p):
        return [p[0] / scale, p[1] / scale, p[2] / scale]

    lw = LEFT_WRIST
    rw = RIGHT_WRIST
    n_left = n_right = 0
    for i, p in enumerate(pos):
        best = max(range(4), key=lambda k: weights[i][k])
        j = joints[i][best]
        w = weights[i][best]
        bb = to_bb(p)
        if j == li and w > 0.35:
            pos[i] = to_gltf(rot_x_neg90(bb, lw))
            n_left += 1
        elif j == ri and w > 0.35:
            pos[i] = to_gltf(rot_x_pos90(bb, rw))
            n_right += 1
        elif j == lla and w > 0.35 and bb[0] > 18.0:
            pos[i] = to_gltf(rot_x_neg90(bb, lw))
            n_left += 1
        elif j == rla and w > 0.35 and bb[0] < -18.0:
            pos[i] = to_gltf(rot_x_pos90(bb, rw))
            n_right += 1
        else:
            continue

    # Rotate matching normals with the same wrist X rolls (use updated joint test via weights).
    if "NORMAL" in prim["attributes"]:
        nrm = [list(n) for n in read_gltf_acc(g, raw, prim["attributes"]["NORMAL"])]
        for i in range(len(nrm)):
            best = max(range(4), key=lambda k: weights[i][k])
            j = joints[i][best]
            w = weights[i][best]
            # Re-derive from original? We mutated pos — use weight membership only for hand bones.
            nx, ny, nz = nrm[i]
            if j == li and w > 0.35:
                nrm[i] = [nx, nz, -ny]
            elif j == ri and w > 0.35:
                nrm[i] = [nx, -nz, ny]
            elif j == lla and w > 0.35:
                # only outer forearm was moved; approximate: if new x still past wrist
                if pos[i][0] * scale > 18.0:
                    nrm[i] = [nx, nz, -ny]
            elif j == rla and w > 0.35:
                if pos[i][0] * scale < -18.0:
                    nrm[i] = [nx, -nz, ny]
        write_gltf_f32_vec3(g, raw, prim["attributes"]["NORMAL"], nrm)

    write_gltf_f32_vec3(g, raw, prim["attributes"]["POSITION"], pos)

    g["buffers"][0]["uri"] = (
        "data:application/octet-stream;base64," + base64.b64encode(bytes(raw)).decode("ascii")
    )
    g["buffers"][0]["byteLength"] = len(raw)
    print(f"glTF flattened verts left={n_left} right={n_right}")
    return g


def main():
    import argparse

    ap = argparse.ArgumentParser()
    ap.add_argument("--gltf-only", action="store_true", help="Skip bbmodel (already flattened)")
    ap.add_argument("--bb-only", action="store_true", help="Skip glTF")
    args = ap.parse_args()

    if not args.gltf_only:
        bb = flatten_bbmodel()
        BB.write_text(json.dumps(bb, separators=(",", ":")), encoding="utf-8")
        print("Wrote", BB)
        art_bb = REPO / "tools/art/player/Player_V2_rigged.bbmodel"
        art_bb.parent.mkdir(parents=True, exist_ok=True)
        art_bb.write_text(json.dumps(bb, separators=(",", ":")), encoding="utf-8")

    if not args.bb_only:
        if GLTF.exists():
            g = flatten_gltf()
            GLTF.write_text(json.dumps(g, separators=(",", ":")), encoding="utf-8")
            print("Wrote", GLTF)
        else:
            print("No glTF at", GLTF, "- bake will need a Blockbench re-export")


if __name__ == "__main__":
    main()
