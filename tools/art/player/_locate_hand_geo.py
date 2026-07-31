"""Locate orphaned hand geometry by position clusters near hand bones."""
from __future__ import annotations

import base64
import json
import struct
from collections import defaultdict
from pathlib import Path


def read_blob(g):
    return base64.b64decode(g["buffers"][0]["uri"].split(",", 1)[1])


def read_accessor(g, raw, acc_idx):
    acc = g["accessors"][acc_idx]
    bv = g["bufferViews"][acc["bufferView"]]
    offset = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    ctype = acc["componentType"]
    typ = acc["type"]
    comps = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[typ]
    size = {5126: 4, 5123: 2, 5121: 1}[ctype]
    stride = bv.get("byteStride", size * comps)
    fmt = {5126: "f", 5123: "H", 5121: "B"}[ctype]
    out = []
    for i in range(acc["count"]):
        o = offset + i * stride
        vals = struct.unpack_from("<" + fmt * comps, raw, o)
        out.append(vals if comps > 1 else vals[0])
    return out


def node_worlds(g):
    def mul(a, b):
        m = [0.0] * 16
        for c in range(4):
            for r in range(4):
                m[c * 4 + r] = sum(a[k * 4 + r] * b[c * 4 + k] for k in range(4))
        return m

    def local(n):
        m = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
        if "matrix" in n:
            return list(n["matrix"])
        if "scale" in n:
            sx, sy, sz = n["scale"]
            m = mul([sx, 0, 0, 0, 0, sy, 0, 0, 0, 0, sz, 0, 0, 0, 0, 1], m)
        if "rotation" in n:
            x, y, z, w = n["rotation"]
            xx, yy, zz = x * x, y * y, z * z
            xy, xz, yz = x * y, x * z, y * z
            wx, wy, wz = w * x, w * y, w * z
            rm = [
                1 - 2 * (yy + zz), 2 * (xy + wz), 2 * (xz - wy), 0,
                2 * (xy - wz), 1 - 2 * (xx + zz), 2 * (yz + wx), 0,
                2 * (xz + wy), 2 * (yz - wx), 1 - 2 * (xx + yy), 0,
                0, 0, 0, 1,
            ]
            m = mul(rm, m)
        if "translation" in n:
            tx, ty, tz = n["translation"]
            m = mul([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, tx, ty, tz, 1], m)
        return m

    parents = {i: None for i in range(len(g["nodes"]))}
    for i, n in enumerate(g["nodes"]):
        for c in n.get("children") or []:
            parents[c] = i
    locals_m = [local(n) for n in g["nodes"]]
    worlds = [None] * len(g["nodes"])

    def compute(i, pm):
        worlds[i] = mul(pm, locals_m[i]) if pm else locals_m[i]
        for c in g["nodes"][i].get("children") or []:
            compute(c, worlds[i])

    ident = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    for i, p in parents.items():
        if p is None:
            compute(i, ident)
    return worlds


def main():
    path = Path(r"C:/Users/johnr/Documents/3DGameIdea/samples/open-world-rpg/assets/models/player.gltf")
    g = json.loads(path.read_text(encoding="utf-8"))
    raw = read_blob(g)
    prim = g["meshes"][0]["primitives"][0]
    pos = read_accessor(g, raw, prim["attributes"]["POSITION"])
    joints = read_accessor(g, raw, prim["attributes"]["JOINTS_0"])
    weights = read_accessor(g, raw, prim["attributes"]["WEIGHTS_0"])
    names = [g["nodes"][i].get("name") for i in g["skins"][0]["joints"]]
    worlds = node_worlds(g)
    name_to_node = {n.get("name"): i for i, n in enumerate(g["nodes"]) if n.get("name")}

    for bname in ["LeftHand", "LeftIndex1", "LeftIndex2", "LeftThumb1", "LeftPinky2"]:
        ni = name_to_node[bname]
        ox, oy, oz = worlds[ni][12], worlds[ni][13], worlds[ni][14]
        near = []
        for i, p in enumerate(pos):
            d2 = (p[0] - ox) ** 2 + (p[1] - oy) ** 2 + (p[2] - oz) ** 2
            if d2 < 0.15**2:
                js = [int(joints[i][k]) for k in range(4)]
                ws = [float(weights[i][k]) for k in range(4)]
                parts = [f"{names[js[k]]}={ws[k]:.2f}" for k in range(4) if ws[k] > 0.01]
                near.append((d2**0.5, p, parts))
        near.sort()
        print(f"{bname} origin=({ox:.2f},{oy:.2f},{oz:.2f}) nearby={len(near)}")
        for d, p, parts in near[:5]:
            print(f"  d={d:.3f} p=({p[0]:.2f},{p[1]:.2f},{p[2]:.2f}) {parts}")

    # count verts with |x| > 1.25 (beyond typical forearm tip ~1.2 lowerarm)
    far = [p for p in pos if abs(p[0]) > 1.30]
    print(f"\nverts |x|>1.30: {len(far)}")
    if far:
        print("  y range", min(p[1] for p in far), max(p[1] for p in far))

    # weight sum sanity
    bad = 0
    for i in range(len(pos)):
        s = sum(float(weights[i][k]) for k in range(4))
        if abs(s - 1.0) > 0.05:
            bad += 1
    print("bad weight sums:", bad)


if __name__ == "__main__":
    main()
