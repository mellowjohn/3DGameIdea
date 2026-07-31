"""Diagnose missing hands: bind-pose positions and skin weights for distal arm verts."""
from __future__ import annotations

import base64
import json
import struct
from pathlib import Path


def read_blob(g: dict) -> bytes:
    uri = g["buffers"][0]["uri"]
    assert uri.startswith("data:application/octet-stream;base64,")
    return base64.b64decode(uri.split(",", 1)[1])


def read_accessor(g: dict, raw: bytes, acc_idx: int):
    acc = g["accessors"][acc_idx]
    bv = g["bufferViews"][acc["bufferView"]]
    offset = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    ctype = acc["componentType"]
    typ = acc["type"]
    comps = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[typ]
    size = {5126: 4, 5123: 2, 5121: 1, 5125: 4, 5120: 1}[ctype]
    stride = bv.get("byteStride", size * comps)
    fmt = {5126: "f", 5123: "H", 5121: "B", 5125: "I", 5120: "b"}[ctype]
    out = []
    for i in range(acc["count"]):
        o = offset + i * stride
        vals = struct.unpack_from("<" + fmt * comps, raw, o)
        out.append(vals if comps > 1 else vals[0])
    return out, acc


def mat4_mul(a, b):
    m = [0.0] * 16
    for c in range(4):
        for r in range(4):
            m[c * 4 + r] = (
                a[0 * 4 + r] * b[c * 4 + 0]
                + a[1 * 4 + r] * b[c * 4 + 1]
                + a[2 * 4 + r] * b[c * 4 + 2]
                + a[3 * 4 + r] * b[c * 4 + 3]
            )
    return m


def translate_matrix(t):
    x, y, z = t
    return [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1]


def quat_matrix(q):
    x, y, z, w = q
    xx, yy, zz = x * x, y * y, z * z
    xy, xz, yz = x * y, x * z, y * z
    wx, wy, wz = w * x, w * y, w * z
    return [
        1 - 2 * (yy + zz),
        2 * (xy + wz),
        2 * (xz - wy),
        0,
        2 * (xy - wz),
        1 - 2 * (xx + zz),
        2 * (yz + wx),
        0,
        2 * (xz + wy),
        2 * (yz - wx),
        1 - 2 * (xx + yy),
        0,
        0,
        0,
        0,
        1,
    ]


def scale_matrix(s):
    x, y, z = s
    return [x, 0, 0, 0, 0, y, 0, 0, 0, 0, z, 0, 0, 0, 0, 1]


def node_local(n):
    m = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    if "matrix" in n:
        return list(n["matrix"])
    if "scale" in n:
        m = mat4_mul(scale_matrix(n["scale"]), m)
    if "rotation" in n:
        m = mat4_mul(quat_matrix(n["rotation"]), m)
    if "translation" in n:
        m = mat4_mul(translate_matrix(n["translation"]), m)
    return m


def mat4_transform_point(m, p):
    x, y, z = p
    return (
        m[0] * x + m[4] * y + m[8] * z + m[12],
        m[1] * x + m[5] * y + m[9] * z + m[13],
        m[2] * x + m[6] * y + m[10] * z + m[14],
    )


def mat4_invert(m):
    # general 4x4 invert
    inv = [0.0] * 16
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10]
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10]
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9]
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9]
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10]
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10]
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9]
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9]
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6]
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6]
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5]
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5]
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6]
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6]
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5]
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5]
    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12]
    if abs(det) < 1e-12:
        raise ValueError("singular")
    return [v / det for v in inv]


def analyze(path: Path) -> None:
    g = json.loads(path.read_text(encoding="utf-8"))
    raw = read_blob(g)
    prim = g["meshes"][0]["primitives"][0]
    pos, _ = read_accessor(g, raw, prim["attributes"]["POSITION"])
    joints_a, jacc = read_accessor(g, raw, prim["attributes"]["JOINTS_0"])
    weights, _ = read_accessor(g, raw, prim["attributes"]["WEIGHTS_0"])
    skin = g["skins"][0]
    joint_nodes = skin["joints"]
    joint_names = [g["nodes"][i].get("name") for i in joint_nodes]
    ibm, _ = read_accessor(g, raw, skin["inverseBindMatrices"])

    # world matrices
    children = {i: [] for i in range(len(g["nodes"]))}
    parents = {i: None for i in range(len(g["nodes"]))}
    for i, n in enumerate(g["nodes"]):
        for c in n.get("children") or []:
            children[i].append(c)
            parents[c] = i
    locals_m = [node_local(n) for n in g["nodes"]]
    worlds = [None] * len(g["nodes"])

    def compute(i, parent_m):
        worlds[i] = mat4_mul(parent_m, locals_m[i]) if parent_m else locals_m[i]
        for c in children[i]:
            compute(c, worlds[i])

    roots = [i for i, p in parents.items() if p is None]
    identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    for r in roots:
        compute(r, identity)

    # skin matrices bind pose: world * ibm
    skin_m = [mat4_mul(worlds[ji], list(ibm[k])) for k, ji in enumerate(joint_nodes)]

    # find far X verts (hands)
    idxs = sorted(range(len(pos)), key=lambda i: abs(pos[i][0]), reverse=True)[:40]
    print("===", path.name, "verts", len(pos))
    print("pos X range", min(p[0] for p in pos), max(p[0] for p in pos))
    print("top |X| verts (bind POSITION -> skinned):")
    for i in idxs[:12]:
        p = pos[i]
        js = joints_a[i]
        ws = weights[i]
        # normalize uint joints if needed
        if jacc["componentType"] in (5121, 5123):
            jidx = [int(js[k]) for k in range(4)]
        else:
            jidx = [int(js[k]) for k in range(4)]
        skinned = [0.0, 0.0, 0.0]
        parts = []
        for k in range(4):
            w = float(ws[k])
            if w <= 1e-6:
                continue
            ji = jidx[k]
            sm = skin_m[ji]
            tp = mat4_transform_point(sm, p)
            skinned[0] += tp[0] * w
            skinned[1] += tp[1] * w
            skinned[2] += tp[2] * w
            parts.append(f"{joint_names[ji]}={w:.2f}")
        print(
            f"  i={i} bind=({p[0]:.2f},{p[1]:.2f},{p[2]:.2f}) "
            f"skinned=({skinned[0]:.2f},{skinned[1]:.2f},{skinned[2]:.2f}) "
            f"[{', '.join(parts)}]"
        )

    # count verts primarily influenced by hand/finger bones
    hand_names = {n for n in joint_names if any(k in n for k in ("Hand", "Thumb", "Index", "Middle", "Ring", "Pinky"))}
    hand_idx = {i for i, n in enumerate(joint_names) if n in hand_names}
    hand_vert = 0
    for i in range(len(pos)):
        js = joints_a[i]
        ws = weights[i]
        wsum = 0.0
        for k in range(4):
            if int(js[k]) in hand_idx:
                wsum += float(ws[k])
        if wsum > 0.5:
            hand_vert += 1
    print("verts majority-weighted to hand/finger bones:", hand_vert)

    # check LeftHand world translation
    for name in ("LeftHand", "RightHand", "LeftUpperArm", "Hips"):
        ji = joint_names.index(name)
        node = joint_nodes[ji]
        t = worlds[node][12:15]
        print(f"  world origin {name}: ({t[0]:.3f},{t[1]:.3f},{t[2]:.3f})")


if __name__ == "__main__":
    analyze(Path(r"C:/Users/johnr/Documents/Models/GoodPlayerModel.gltf"))
    print()
    analyze(Path(r"C:/Users/johnr/Documents/3DGameIdea/samples/open-world-rpg/assets/models/player.gltf"))
