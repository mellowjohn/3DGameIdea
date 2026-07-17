"""Bake Blockbench Player.gltf into an engine-ready static mesh.

The engine mesh importer does not yet bake node transforms or sample textures.
This script flattens hierarchy, samples the embedded atlas into COLOR_0, centers
feet at y=0, and scales height to ~1.8m.
"""

from __future__ import annotations

import base64
import io
import json
import struct
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[1]
SRC = REPO / "tools/art/player/player.blockbench.gltf"
DST = REPO / "samples/open-world-rpg/assets/models/player.gltf"
PNG_DST = DST.parent / "player.png"
TARGET_HEIGHT = 1.8


def read_accessor(g: dict, acc_idx: int):
    acc = g["accessors"][acc_idx]
    bv = g["bufferViews"][acc["bufferView"]]
    buf = g["buffers"][bv["buffer"]]
    uri = buf["uri"]
    assert uri.startswith("data:application/octet-stream;base64,")
    raw = base64.b64decode(uri.split(",", 1)[1])
    offset = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    ctype = acc["componentType"]
    typ = acc["type"]
    count = acc["count"]
    comps = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[typ]
    if ctype == 5126:
        fmt, size = "f", 4
    elif ctype == 5123:
        fmt, size = "H", 2
    elif ctype == 5125:
        fmt, size = "I", 4
    else:
        raise RuntimeError(f"unsupported componentType {ctype}")
    stride = bv.get("byteStride", size * comps)
    out = []
    for i in range(count):
        o = offset + i * stride
        vals = struct.unpack_from("<" + fmt * comps, raw, o)
        out.append(vals if comps > 1 else vals[0])
    return out


def mat_identity():
    return [[1.0, 0.0, 0.0, 0.0], [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 1.0, 0.0], [0.0, 0.0, 0.0, 1.0]]


def mat_mul(a, b):
    m = [[0.0] * 4 for _ in range(4)]
    for r in range(4):
        for c in range(4):
            m[r][c] = sum(a[r][k] * b[k][c] for k in range(4))
    return m


def mat_from_node(n: dict):
    if "matrix" in n:
        vals = n["matrix"]
        return [[vals[c * 4 + r] for c in range(4)] for r in range(4)]
    t = mat_identity()
    if "translation" in n:
        x, y, z = n["translation"]
        t[0][3], t[1][3], t[2][3] = x, y, z
    r = mat_identity()
    if "rotation" in n:
        qx, qy, qz, qw = n["rotation"]
        xx, yy, zz = qx * qx, qy * qy, qz * qz
        xy, xz, yz = qx * qy, qx * qz, qy * qz
        wx, wy, wz = qw * qx, qw * qy, qw * qz
        r = [
            [1 - 2 * (yy + zz), 2 * (xy - wz), 2 * (xz + wy), 0.0],
            [2 * (xy + wz), 1 - 2 * (xx + zz), 2 * (yz - wx), 0.0],
            [2 * (xz - wy), 2 * (yz + wx), 1 - 2 * (xx + yy), 0.0],
            [0.0, 0.0, 0.0, 1.0],
        ]
    s = mat_identity()
    if "scale" in n:
        sx, sy, sz = n["scale"]
        s[0][0], s[1][1], s[2][2] = sx, sy, sz
    return mat_mul(t, mat_mul(r, s))


def transform_point(m, p):
    x, y, z = p
    return (
        m[0][0] * x + m[0][1] * y + m[0][2] * z + m[0][3],
        m[1][0] * x + m[1][1] * y + m[1][2] * z + m[1][3],
        m[2][0] * x + m[2][1] * y + m[2][2] * z + m[2][3],
    )


def pad4(b: bytes) -> bytes:
    return b + (b"\x00" * ((4 - (len(b) % 4)) % 4))


def main() -> None:
    g = json.loads(SRC.read_text(encoding="utf-8"))
    img_uri = g["images"][0]["uri"]
    assert img_uri.startswith("data:image/png;base64,")
    tex = Image.open(io.BytesIO(base64.b64decode(img_uri.split(",", 1)[1]))).convert("RGBA")
    tw, th = tex.size

    def sample_uv(u: float, v: float):
        def at(uu: float, vv: float):
            x = max(0, min(tw - 1, int(uu * tw)))
            y = max(0, min(th - 1, int(vv * th)))
            return tex.getpixel((x, y))

        r, gch, b, a = at(u, v)
        if a < 8:
            r, gch, b, a = at(u, 1.0 - v)
        return (r / 255.0, gch / 255.0, b / 255.0, max(a / 255.0, 1.0))

    world = [None] * len(g["nodes"])

    def bake_world(idx: int, parent):
        local = mat_from_node(g["nodes"][idx])
        world[idx] = mat_mul(parent, local) if parent else local
        for child in g["nodes"][idx].get("children", []):
            bake_world(child, world[idx])

    for root in g["scenes"][0]["nodes"]:
        bake_world(root, None)

    positions = []
    colors = []
    uv_coords = []
    indices = []

    for ni, node in enumerate(g["nodes"]):
        if "mesh" not in node or world[ni] is None:
            continue
        mesh = g["meshes"][node["mesh"]]
        m = world[ni]
        for prim in mesh["primitives"]:
            attrs = prim["attributes"]
            pos = read_accessor(g, attrs["POSITION"])
            uvs = (
                read_accessor(g, attrs["TEXCOORD_0"])
                if "TEXCOORD_0" in attrs
                else [(0.5, 0.5)] * len(pos)
            )
            idx = read_accessor(g, prim["indices"]) if "indices" in prim else list(range(len(pos)))
            base = len(positions)
            for p, uv in zip(pos, uvs):
                positions.append(transform_point(m, p))
                colors.append(sample_uv(uv[0], uv[1]))
                # glTF UV convention (top-left origin); the engine samples with V as-is.
                uv_coords.append((float(uv[0]), float(uv[1])))
            for i in idx:
                indices.append(base + i)

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
    print(
        f"raw AABB x[{minx:.4f},{maxx:.4f}] y[{miny:.4f},{maxy:.4f}] "
        f"z[{minz:.4f},{maxz:.4f}] h={height:.4f}"
    )
    print(f"scale={scale:.4f} center xz=({cx:.4f},{cz:.4f})")

    norm_pos = [((x - cx) * scale, (y - miny) * scale, (z - cz) * scale) for x, y, z in positions]

    pos_bytes = pad4(b"".join(struct.pack("<fff", *p) for p in norm_pos))
    col_bytes = pad4(b"".join(struct.pack("<ffff", *c) for c in colors))
    uv_bytes = pad4(b"".join(struct.pack("<ff", *uv) for uv in uv_coords))
    if len(norm_pos) > 65535:
        raise RuntimeError("too many vertices for UNSIGNED_SHORT indices")
    idx_bytes = pad4(b"".join(struct.pack("<H", i) for i in indices))
    blob = pos_bytes + col_bytes + uv_bytes + idx_bytes
    b64 = base64.b64encode(blob).decode("ascii")

    # Write the atlas next to the glTF so the engine can sample GPU albedo (point/clamp).
    tex.save(PNG_DST, format="PNG")

    nverts = len(norm_pos)
    nidx = len(indices)
    nys = [p[1] for p in norm_pos]
    nxs = [p[0] for p in norm_pos]
    nzs = [p[2] for p in norm_pos]

    out = {
        "asset": {
            "version": "2.0",
            "generator": "AI RPG Engine player bake from Blockbench Player.gltf v1",
        },
        "scenes": [{"nodes": [0], "name": "Player"}],
        "scene": 0,
        "nodes": [{"name": "Player", "mesh": 0}],
        "meshes": [
            {
                "name": "Player",
                "primitives": [
                    {
                        "attributes": {"POSITION": 0, "COLOR_0": 1, "TEXCOORD_0": 2},
                        "indices": 3,
                        "material": 0,
                        "mode": 4,
                    }
                ],
            }
        ],
        "materials": [
            {
                "name": "PlayerAtlas",
                "pbrMetallicRoughness": {
                    "baseColorTexture": {"index": 0},
                    "metallicFactor": 0.0,
                    "roughnessFactor": 1.0,
                },
            }
        ],
        "textures": [{"name": "PlayerAtlas", "source": 0, "sampler": 0}],
        "images": [{"name": "PlayerAtlas", "uri": "player.png"}],
        "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 33071}],
        "accessors": [
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
            {"bufferView": 3, "componentType": 5123, "count": nidx, "type": "SCALAR"},
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(pos_bytes), "target": 34962},
            {
                "buffer": 0,
                "byteOffset": len(pos_bytes),
                "byteLength": len(col_bytes),
                "target": 34962,
            },
            {
                "buffer": 0,
                "byteOffset": len(pos_bytes) + len(col_bytes),
                "byteLength": len(uv_bytes),
                "target": 34962,
            },
            {
                "buffer": 0,
                "byteOffset": len(pos_bytes) + len(col_bytes) + len(uv_bytes),
                "byteLength": len(idx_bytes),
                "target": 34963,
            },
        ],
        "buffers": [
            {"byteLength": len(blob), "uri": "data:application/octet-stream;base64," + b64}
        ],
    }

    DST.parent.mkdir(parents=True, exist_ok=True)
    DST.write_text(json.dumps(out, separators=(",", ":")), encoding="utf-8")
    print(f"wrote {DST} ({DST.stat().st_size} bytes) verts={nverts} tris={nidx // 3}")
    print(f"wrote {PNG_DST} ({PNG_DST.stat().st_size} bytes) atlas={tw}x{th}")
    print(
        f"final AABB x[{min(nxs):.3f},{max(nxs):.3f}] y[{min(nys):.3f},{max(nys):.3f}] "
        f"z[{min(nzs):.3f},{max(nzs):.3f}]"
    )


if __name__ == "__main__":
    main()
