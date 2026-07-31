"""Count verts influenced by each joint; find finger geometry."""
from __future__ import annotations

import base64
import json
import struct
from collections import defaultdict
from pathlib import Path


def read_blob(g: dict) -> bytes:
    uri = g["buffers"][0]["uri"]
    return base64.b64decode(uri.split(",", 1)[1])


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


def main():
    path = Path(r"C:/Users/johnr/Documents/3DGameIdea/samples/open-world-rpg/assets/models/player.gltf")
    g = json.loads(path.read_text(encoding="utf-8"))
    raw = read_blob(g)
    prim = g["meshes"][0]["primitives"][0]
    pos = read_accessor(g, raw, prim["attributes"]["POSITION"])
    joints = read_accessor(g, raw, prim["attributes"]["JOINTS_0"])
    weights = read_accessor(g, raw, prim["attributes"]["WEIGHTS_0"])
    names = [g["nodes"][i].get("name") for i in g["skins"][0]["joints"]]

    influence = defaultdict(float)
    primary = defaultdict(int)
    for i in range(len(pos)):
        best_j, best_w = -1, -1.0
        for k in range(4):
            w = float(weights[i][k])
            j = int(joints[i][k])
            if w > 0:
                influence[names[j]] += w
            if w > best_w:
                best_w, best_j = w, j
        primary[names[best_j]] += 1

    print("primary bone (highest weight) counts:")
    for n, c in sorted(primary.items(), key=lambda x: -x[1]):
        print(f"  {n:20s} {c:5d}")

    print("\nfinger/hand primary:")
    for n in names:
        if any(k in n for k in ("Hand", "Thumb", "Index", "Middle", "Ring", "Pinky")):
            print(f"  {n:20s} primary={primary.get(n,0)} influence={influence.get(n,0):.1f}")

    # any vert with any finger weight?
    finger_idx = {i for i, n in enumerate(names) if any(k in n for k in ("Thumb", "Index", "Middle", "Ring", "Pinky"))}
    any_finger = 0
    samples = []
    for i in range(len(pos)):
        for k in range(4):
            if int(joints[i][k]) in finger_idx and float(weights[i][k]) > 0.01:
                any_finger += 1
                if len(samples) < 8:
                    parts = []
                    for kk in range(4):
                        w = float(weights[i][kk])
                        if w > 0.01:
                            parts.append(f"{names[int(joints[i][kk])] }={w:.2f}")
                    samples.append((pos[i], parts))
                break
    print("\nverts with any finger weight:", any_finger)
    for p, parts in samples:
        print(f"  pos=({p[0]:.2f},{p[1]:.2f},{p[2]:.2f}) {parts}")

    # LeftHand primary verts bbox
    lh = names.index("LeftHand")
    pts = []
    for i in range(len(pos)):
        js = [int(joints[i][k]) for k in range(4)]
        ws = [float(weights[i][k]) for k in range(4)]
        if js[ws.index(max(ws))] == lh:
            pts.append(pos[i])
    if pts:
        xs, ys, zs = zip(*pts)
        print(f"\nLeftHand-primary bbox x[{min(xs):.2f},{max(xs):.2f}] y[{min(ys):.2f},{max(ys):.2f}] z[{min(zs):.2f},{max(zs):.2f}] n={len(pts)}")


if __name__ == "__main__":
    main()
