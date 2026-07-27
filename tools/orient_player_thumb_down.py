"""Orient Player_V2 hands: thumb-side down + heal wrist stretch.

Current Documents rig is already palm-flat (one wrist X roll). This applies a
second roll so the thumb edge faces the ground, rotates the body palm with a
smooth forearm falloff (fixes the upright palm-block vs flat-finger stretch),
and nudges the hand inward to close the wrist gap.
"""
from __future__ import annotations

import argparse
import base64
import json
import math
import struct
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
BB = Path(r"c:\Users\johnr\Documents\Player_V2_rigged.bbmodel")
GLTF = Path(r"c:\Users\johnr\Documents\Player_V2_rigged.gltf")

LEFT_WRIST = (21.0, 29.5, 1.2)
RIGHT_WRIST = (-21.0, 29.5, 1.2)
# Forearm falloff in Blockbench units (|x| from spine)
LEFT_ELBOW_X = 14.5
RIGHT_ELBOW_X = -14.5


def rot_x_neg90(p, pivot):
    x, y, z = p[0] - pivot[0], p[1] - pivot[1], p[2] - pivot[2]
    return [x + pivot[0], z + pivot[1], -y + pivot[2]]


def rot_x_pos90(p, pivot):
    x, y, z = p[0] - pivot[0], p[1] - pivot[1], p[2] - pivot[2]
    return [x + pivot[0], -z + pivot[1], y + pivot[2]]


def lerp(a, b, t):
    return a + (b - a) * t


def slerp_rot_about_x(p, pivot, degrees):
    """Rotate p about pivot's X-parallel axis by degrees (can be partial)."""
    rad = math.radians(degrees)
    c, s = math.cos(rad), math.sin(rad)
    x, y, z = p[0] - pivot[0], p[1] - pivot[1], p[2] - pivot[2]
    return [x + pivot[0], c * y - s * z + pivot[1], s * y + c * z + pivot[2]]


def world_point(origin, rotation, local):
    rx, ry, rz = [math.radians(a) for a in (rotation or [0, 0, 0])]
    cx, sx = math.cos(rx), math.sin(rx)
    cy, sy = math.cos(ry), math.sin(ry)
    cz, sz = math.cos(rz), math.sin(rz)
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


def left_blend(x: float) -> float:
    """0 at elbow, 1 at wrist+."""
    return max(0.0, min(1.0, (x - LEFT_ELBOW_X) / (LEFT_WRIST[0] - LEFT_ELBOW_X)))


def right_blend(x: float) -> float:
    return max(0.0, min(1.0, (x - RIGHT_ELBOW_X) / (RIGHT_WRIST[0] - RIGHT_ELBOW_X)))


def smooth(t: float) -> float:
    return t * t * (3.0 - 2.0 * t)


def orient_point_left(p):
    """Second -90° X: thumb edge from -Z down to -Y. Full at wrist."""
    return rot_x_neg90(p, LEFT_WRIST)


def orient_point_right(p):
    return rot_x_pos90(p, RIGHT_WRIST)


def orient_with_falloff_left(p):
    b = smooth(left_blend(p[0]))
    if b <= 0:
        return p
    if b >= 1:
        return orient_point_left(p)
    # Partial -90° = rotate by -90*b degrees
    return slerp_rot_about_x(p, LEFT_WRIST, -90.0 * b)


def orient_with_falloff_right(p):
    b = smooth(right_blend(p[0]))
    if b <= 0:
        return p
    if b >= 1:
        return orient_point_right(p)
    return slerp_rot_about_x(p, RIGHT_WRIST, 90.0 * b)


def close_wrist_gap_left(p, pull: float):
    """Pull hand/finger verts slightly toward elbow to close authored gap."""
    if p[0] < LEFT_WRIST[0] - 0.5:
        return p
    return [p[0] - pull, p[1], p[2]]


def close_wrist_gap_right(p, pull: float):
    if p[0] > RIGHT_WRIST[0] + 0.5:
        return p
    return [p[0] + pull, p[1], p[2]]


def edit_bbmodel(pull: float = 0.6):
    data = json.loads(BB.read_text(encoding="utf-8"))
    finger_names = {f"LeftFinger{i}" for i in range(1, 7)} | {f"RightFinger{i}" for i in range(1, 7)}

    for e in data["elements"]:
        name = e.get("name", "")
        o = e.get("origin") or [0, 0, 0]
        rot = e.get("rotation") or [0, 0, 0]
        verts = e.get("vertices") or {}
        if not verts:
            continue

        if name in finger_names:
            left = name.startswith("Left")
            new_verts = {}
            for vid, local in verts.items():
                w = world_point(o, rot, local)
                w2 = orient_point_left(w) if left else orient_point_right(w)
                w2 = close_wrist_gap_left(w2, pull) if left else close_wrist_gap_right(w2, pull)
                new_verts[vid] = [w2[0] - o[0], w2[1] - o[1], w2[2] - o[2]]
            e["vertices"] = new_verts
            e["rotation"] = [0, 0, 0]
            print(f"{name}: thumb-down + wrist pull")
            continue

        if name != "BodyMesh":
            continue

        # Palm + forearm falloff on the continuous body mesh
        new_verts = {}
        n_l = n_r = 0
        for vid, local in verts.items():
            w = world_point(o, rot, local)
            if w[0] > LEFT_ELBOW_X and w[1] > 24.0:
                w2 = orient_with_falloff_left(w)
                if left_blend(w[0]) > 0.85:
                    w2 = close_wrist_gap_left(w2, pull)
                n_l += 1
            elif w[0] < RIGHT_ELBOW_X and w[1] > 24.0:
                w2 = orient_with_falloff_right(w)
                if right_blend(w[0]) > 0.85:
                    w2 = close_wrist_gap_right(w2, pull)
                n_r += 1
            else:
                w2 = w
            new_verts[vid] = [w2[0] - o[0], w2[1] - o[1], w2[2] - o[2]]
        e["vertices"] = new_verts
        print(f"BodyMesh: falloff-oriented arm/hand verts L={n_l} R={n_r}")

    BB.write_text(json.dumps(data, separators=(",", ":")), encoding="utf-8")
    art = REPO / "tools/art/player/Player_V2_rigged.bbmodel"
    art.write_text(json.dumps(data, separators=(",", ":")), encoding="utf-8")
    print("Wrote", BB)


def read_acc(g, raw, idx):
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
        out.append(list(vals) if comps > 1 else vals[0])
    return out


def write_vec3(g, raw, idx, values):
    acc = g["accessors"][idx]
    bv = g["bufferViews"][acc["bufferView"]]
    off = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    stride = bv.get("byteStride", 12)
    for i, v in enumerate(values):
        struct.pack_into("<fff", raw, off + i * stride, float(v[0]), float(v[1]), float(v[2]))
    xs, ys, zs = [v[0] for v in values], [v[1] for v in values], [v[2] for v in values]
    acc["min"] = [min(xs), min(ys), min(zs)]
    acc["max"] = [max(xs), max(ys), max(zs)]


def edit_gltf(pull_bb: float = 0.6):
    g = json.loads(GLTF.read_text(encoding="utf-8"))
    raw = bytearray(base64.b64decode(g["buffers"][0]["uri"].split(",", 1)[1]))
    prim = g["meshes"][0]["primitives"][0]
    pos = read_acc(g, raw, prim["attributes"]["POSITION"])
    nrm = read_acc(g, raw, prim["attributes"]["NORMAL"])
    joints = read_acc(g, raw, prim["attributes"]["JOINTS_0"])
    weights = read_acc(g, raw, prim["attributes"]["WEIGHTS_0"])
    names = [g["nodes"][j].get("name") for j in g["skins"][0]["joints"]]
    li = names.index("LeftHand")
    ri = names.index("RightHand")
    lla = names.index("LeftLowerArm")
    rla = names.index("RightLowerArm")
    lua = names.index("LeftUpperArm")
    rua = names.index("RightUpperArm")

    hand_xs = [p[0] for i, p in enumerate(pos) if joints[i][0] == li and weights[i][0] > 0.5]
    scale = abs(LEFT_WRIST[0] / sorted(hand_xs)[len(hand_xs) // 2])
    print(f"glTF scale~{scale:.4f}")
    pull = pull_bb / scale

    def to_bb(p):
        return [p[0] * scale, p[1] * scale, p[2] * scale]

    def to_g(p):
        return [p[0] / scale, p[1] / scale, p[2] / scale]

    def bone_w(i, bone):
        return sum(weights[i][k] for k in range(4) if joints[i][k] == bone)

    n = 0
    for i, p in enumerate(pos):
        bb = to_bb(p)
        wh_l = bone_w(i, li) + bone_w(i, lla) * 0.85 + bone_w(i, lua) * 0.15
        wh_r = bone_w(i, ri) + bone_w(i, rla) * 0.85 + bone_w(i, rua) * 0.15
        # Spatial fallback for body-mesh palm still weighted to Chest/etc.
        if bb[0] > LEFT_ELBOW_X and bb[1] > 24.0 and (wh_l > 0.2 or bb[0] > 16.0):
            bb2 = orient_with_falloff_left(bb)
            if left_blend(bb[0]) > 0.85:
                bb2 = close_wrist_gap_left(bb2, pull_bb)
            pos[i] = to_g(bb2)
            b = smooth(left_blend(bb[0]))
            rad = math.radians(-90.0 * b)
            c, s = math.cos(rad), math.sin(rad)
            nx, ny, nz = nrm[i]
            nrm[i] = [nx, c * ny - s * nz, s * ny + c * nz]
            n += 1
        elif bb[0] < RIGHT_ELBOW_X and bb[1] > 24.0 and (wh_r > 0.2 or bb[0] < -16.0):
            bb2 = orient_with_falloff_right(bb)
            if right_blend(bb[0]) > 0.85:
                bb2 = close_wrist_gap_right(bb2, pull_bb)
            pos[i] = to_g(bb2)
            rad = math.radians(90.0 * smooth(right_blend(bb[0])))
            c, s = math.cos(rad), math.sin(rad)
            nx, ny, nz = nrm[i]
            nrm[i] = [nx, c * ny - s * nz, s * ny + c * nz]
            n += 1

    write_vec3(g, raw, prim["attributes"]["POSITION"], pos)
    write_vec3(g, raw, prim["attributes"]["NORMAL"], nrm)
    g["buffers"][0]["uri"] = (
        "data:application/octet-stream;base64," + base64.b64encode(bytes(raw)).decode("ascii")
    )
    g["buffers"][0]["byteLength"] = len(raw)
    GLTF.write_text(json.dumps(g, separators=(",", ":")), encoding="utf-8")
    print(f"Wrote {GLTF} ({n} verts oriented)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pull", type=float, default=0.6, help="BB units to pull hand toward elbow")
    ap.add_argument("--bb-only", action="store_true")
    ap.add_argument("--gltf-only", action="store_true")
    args = ap.parse_args()
    if not args.gltf_only:
        edit_bbmodel(pull=args.pull)
    if not args.bb_only:
        edit_gltf(pull_bb=args.pull)


if __name__ == "__main__":
    main()
