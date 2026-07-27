"""Inject Nose + Brow cubes into Player_V2_rigged.gltf at unrigged world positions.

Blockbench export of the broken cube_to_mesh (absolute verts + origin) dropped or
misplaced those face parts. Rebuild them from the original .bbmodel cubes,
weight 100% to Head, then leave bake_player_v2_gltf.py to normalize scale.
"""
from __future__ import annotations

import base64
import json
import math
import shutil
import struct
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
BB_ORIG = Path(r"c:\Users\johnr\Documents\Player_V2.bbmodel")
GLTF_SRC = Path(r"c:\Users\johnr\Documents\Player_V2_rigged.gltf")
GLTF_FALLBACK = REPO / "tools/art/player/Player_V2_rigged.gltf"
ART = REPO / "tools/art/player"

# Blockbench free-model export uses 1 unit = 1/16? Empirically head top 41.05 → 4.66875
BB_TO_GLTF = 4.668749809265137 / 41.05


def read_blob(g: dict) -> bytearray:
    return bytearray(base64.b64decode(g["buffers"][0]["uri"].split(",", 1)[1]))


def write_blob(g: dict, raw: bytes) -> None:
    g["buffers"][0]["uri"] = (
        "data:application/octet-stream;base64," + base64.b64encode(raw).decode("ascii")
    )
    g["buffers"][0]["byteLength"] = len(raw)


def accessor_meta(g, acc_idx):
    acc = g["accessors"][acc_idx]
    bv = g["bufferViews"][acc["bufferView"]]
    offset = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    comps = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[acc["type"]]
    ctype = acc["componentType"]
    size = {5126: 4, 5123: 2, 5121: 1, 5125: 4}[ctype]
    stride = bv.get("byteStride", size * comps)
    return acc, offset, comps, size, stride, ctype


def read_acc(g, raw, acc_idx):
    acc, offset, comps, size, stride, ctype = accessor_meta(g, acc_idx)
    fmt = {5126: "f", 5123: "H", 5121: "B", 5125: "I"}[ctype]
    out = []
    for i in range(acc["count"]):
        vals = struct.unpack_from("<" + fmt * comps, raw, offset + i * stride)
        out.append(vals if comps > 1 else vals[0])
    return out


def cube_tris(fr, to):
    """Return positions (24 unique-per-face verts), normals, uvs, indices for a box."""
    x0, y0, z0 = [c * BB_TO_GLTF for c in fr]
    x1, y1, z1 = [c * BB_TO_GLTF for c in to]
    faces = [
        # -Z
        ([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)], (0, 0, -1)),
        # +Z
        ([(x1, y0, z1), (x0, y0, z1), (x0, y1, z1), (x1, y1, z1)], (0, 0, 1)),
        # -Y
        ([(x0, y0, z1), (x1, y0, z1), (x1, y0, z0), (x0, y0, z0)], (0, -1, 0)),
        # +Y
        ([(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)], (0, 1, 0)),
        # -X
        ([(x0, y0, z1), (x0, y0, z0), (x0, y1, z0), (x0, y1, z1)], (-1, 0, 0)),
        # +X
        ([(x1, y0, z0), (x1, y0, z1), (x1, y1, z1), (x1, y1, z0)], (1, 0, 0)),
    ]
    pos, nrm, uv, idx = [], [], [], []
    for corners, normal in faces:
        base = len(pos)
        uvs = [(0, 0), (1, 0), (1, 1), (0, 1)]
        for c, t in zip(corners, uvs):
            pos.append(c)
            nrm.append(normal)
            uv.append(t)
        idx.extend([base, base + 1, base + 2, base, base + 2, base + 3])
    return pos, nrm, uv, idx


def append_bytes(raw: bytearray, data: bytes) -> int:
    pad = (4 - (len(raw) % 4)) % 4
    raw.extend(b"\x00" * pad)
    off = len(raw)
    raw.extend(data)
    return off


def main() -> None:
    src_path = GLTF_SRC if GLTF_SRC.exists() else GLTF_FALLBACK
    bb = json.loads(BB_ORIG.read_text(encoding="utf-8"))
    brow_r, brow_l, nose = bb["elements"][2], bb["elements"][3], bb["elements"][5]
    cubes = [
        ("Nose", nose["from"], nose["to"]),
        ("BrowRight", brow_r["from"], brow_r["to"]),
        ("BrowLeft", brow_l["from"], brow_l["to"]),
    ]

    g = json.loads(src_path.read_text(encoding="utf-8"))
    raw = read_blob(g)
    prim = g["meshes"][0]["primitives"][0]
    attrs = prim["attributes"]

    pos = list(read_acc(g, raw, attrs["POSITION"]))
    nrm = list(read_acc(g, raw, attrs["NORMAL"]))
    uvs = list(read_acc(g, raw, attrs["TEXCOORD_0"]))
    joints = list(read_acc(g, raw, attrs["JOINTS_0"]))
    weights = list(read_acc(g, raw, attrs["WEIGHTS_0"]))
    indices = list(read_acc(g, raw, prim["indices"]))

    names = [g["nodes"][j].get("name") for j in g["skins"][0]["joints"]]
    head_j = names.index("Head")

    # Drop prior injected / misplaced face cubes: tiny boxes near expected
    # unrigged nose/brow AABBs (and the doubled-origin floaters if any).
    def is_face_feature(p):
        x, y, z = p
        # unrigged nose/brows in glTF space
        if abs(x) <= 0.05 and 4.15 <= y <= 4.25 and -0.45 <= z <= -0.35:
            return True
        if 0.30 <= abs(x) <= 0.45 and 4.15 <= y <= 4.35 and 0.0 <= z <= 0.10:
            return True
        return False

    keep = [i for i, p in enumerate(pos) if not is_face_feature(p)]
    if len(keep) != len(pos):
        remap = {old: new for new, old in enumerate(keep)}
        pos = [pos[i] for i in keep]
        nrm = [nrm[i] for i in keep]
        uvs = [uvs[i] for i in keep]
        joints = [joints[i] for i in keep]
        weights = [weights[i] for i in keep]
        new_idx = []
        for t in range(0, len(indices), 3):
            tri = indices[t : t + 3]
            if all(v in remap for v in tri):
                new_idx.extend(remap[v] for v in tri)
        indices = new_idx
        print(f"removed {len(remap) and (len(remap) and 'features')}; kept {len(pos)} verts")

    for label, fr, to in cubes:
        # Skip if a cube already exists at the correct unrigged location
        x0, y0, z0 = [c * BB_TO_GLTF for c in fr]
        x1, y1, z1 = [c * BB_TO_GLTF for c in to]
        cx, cy, cz = 0.5 * (x0 + x1), 0.5 * (y0 + y1), 0.5 * (z0 + z1)
        already = any(
            abs(p[0] - cx) < 0.02 and abs(p[1] - cy) < 0.02 and abs(p[2] - cz) < 0.02 for p in pos
        )
        if already:
            print(f"skip {label}: already present near center")
            continue

        p_add, n_add, u_add, i_add = cube_tris(fr, to)
        base = len(pos)
        pos.extend(p_add)
        nrm.extend(n_add)
        uvs.extend(u_add)
        for _ in p_add:
            joints.append((head_j, 0, 0, 0))
            weights.append((1.0, 0.0, 0.0, 0.0))
        indices.extend(base + i for i in i_add)
        print(
            f"added {label}: {len(p_add)} verts at y[{min(p[1] for p in p_add):.3f},"
            f"{max(p[1] for p in p_add):.3f}] Head={head_j}"
        )

    # Rebuild tightly packed buffers (simpler than patching views in place).
    chunks = []
    def add_f32(values, comps):
        data = b"".join(
            struct.pack("<" + "f" * comps, *(v if comps > 1 else (v,)))
            if comps == 1
            else struct.pack("<" + "f" * comps, *v)
            for v in values
        )
        return data

    pos_bytes = b"".join(struct.pack("<fff", *p) for p in pos)
    nrm_bytes = b"".join(struct.pack("<fff", *n) for n in nrm)
    uv_bytes = b"".join(struct.pack("<ff", *u) for u in uvs)
    j_bytes = b"".join(struct.pack("<BBBB", *j) for j in joints)
    # align joints to 4 already
    w_bytes = b"".join(struct.pack("<ffff", *w) for w in weights)
    # indices
    if len(pos) < 65536:
        idx_bytes = b"".join(struct.pack("<H", i) for i in indices)
        idx_ctype = 5123
    else:
        idx_bytes = b"".join(struct.pack("<I", i) for i in indices)
        idx_ctype = 5125

    new_raw = bytearray()
    views = []
    accessors = []

    def push(data: bytes, stride: int | None = None):
        off = append_bytes(new_raw, data)
        view = {"buffer": 0, "byteOffset": off, "byteLength": len(data)}
        if stride:
            view["byteStride"] = stride
        views.append(view)
        return len(views) - 1

    def push_acc(view_idx, count, type_, ctype, mn=None, mx=None, stride=None):
        acc = {
            "bufferView": view_idx,
            "componentType": ctype,
            "count": count,
            "type": type_,
        }
        if mn is not None:
            acc["min"] = mn
        if mx is not None:
            acc["max"] = mx
        accessors.append(acc)
        return len(accessors) - 1

    xs, ys, zs = [p[0] for p in pos], [p[1] for p in pos], [p[2] for p in pos]
    vi = push(pos_bytes, 12)
    ai_pos = push_acc(vi, len(pos), "VEC3", 5126, [min(xs), min(ys), min(zs)], [max(xs), max(ys), max(zs)])
    vi = push(nrm_bytes, 12)
    ai_nrm = push_acc(vi, len(nrm), "VEC3", 5126)
    vi = push(uv_bytes, 8)
    ai_uv = push_acc(vi, len(uvs), "VEC2", 5126)
    vi = push(j_bytes, 4)
    ai_j = push_acc(vi, len(joints), "VEC4", 5121)
    vi = push(w_bytes, 16)
    ai_w = push_acc(vi, len(weights), "VEC4", 5126)
    vi = push(idx_bytes)
    ai_i = push_acc(vi, len(indices), "SCALAR", idx_ctype)

    # Preserve inverse bind + animation accessors by copying their bytes from old raw
    # into the new buffer and remapping indices.
    old_accessors = g["accessors"]
    skin = g["skins"][0]
    ibm_old = skin["inverseBindMatrices"]
    anim_acc_old = set()
    for anim in g.get("animations", []):
        for s in anim["samplers"]:
            anim_acc_old.add(s["input"])
            anim_acc_old.add(s["output"])

    remap_acc = {
        attrs["POSITION"]: ai_pos,
        attrs["NORMAL"]: ai_nrm,
        attrs["TEXCOORD_0"]: ai_uv,
        attrs["JOINTS_0"]: ai_j,
        attrs["WEIGHTS_0"]: ai_w,
        prim["indices"]: ai_i,
    }

    def copy_accessor(old_idx: int) -> int:
        if old_idx in remap_acc:
            return remap_acc[old_idx]
        acc = old_accessors[old_idx]
        bv = g["bufferViews"][acc["bufferView"]]
        start = bv.get("byteOffset", 0)
        data = bytes(raw[start : start + bv["byteLength"]])
        # If accessor has its own byteOffset inside view, keep full view (matches BB export).
        vi = push(data, bv.get("byteStride"))
        new_acc = {
            "bufferView": vi,
            "componentType": acc["componentType"],
            "count": acc["count"],
            "type": acc["type"],
        }
        if "byteOffset" in acc:
            new_acc["byteOffset"] = acc["byteOffset"]
        if "min" in acc:
            new_acc["min"] = acc["min"]
        if "max" in acc:
            new_acc["max"] = acc["max"]
        if "normalized" in acc:
            new_acc["normalized"] = acc["normalized"]
        ni = len(accessors)
        accessors.append(new_acc)
        remap_acc[old_idx] = ni
        return ni

    skin["inverseBindMatrices"] = copy_accessor(ibm_old)
    for anim in g.get("animations", []):
        for s in anim["samplers"]:
            s["input"] = copy_accessor(s["input"])
            s["output"] = copy_accessor(s["output"])

    g["bufferViews"] = views
    g["accessors"] = accessors
    prim["attributes"] = {
        "POSITION": ai_pos,
        "NORMAL": ai_nrm,
        "TEXCOORD_0": ai_uv,
        "JOINTS_0": ai_j,
        "WEIGHTS_0": ai_w,
    }
    prim["indices"] = ai_i
    write_blob(g, bytes(new_raw))

    for dest in (GLTF_SRC, ART / "Player_V2_rigged.gltf"):
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text(json.dumps(g, separators=(",", ":")), encoding="utf-8")
        print("Wrote", dest)

    # sanity
    g2 = json.loads(GLTF_SRC.read_text(encoding="utf-8"))
    raw2 = read_blob(g2)
    pos2 = read_acc(g2, raw2, g2["meshes"][0]["primitives"][0]["attributes"]["POSITION"])
    print(
        f"final verts={len(pos2)} y[{min(p[1] for p in pos2):.3f},{max(p[1] for p in pos2):.3f}]"
    )


if __name__ == "__main__":
    main()
