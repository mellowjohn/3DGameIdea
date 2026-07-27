"""Bake Blockbench Tier-1 prop glTFs (crate, bush, tall bush, campfire, barrel, lantern, wall torch) for the engine."""

from __future__ import annotations

import base64
import io
import json
import struct
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[1]
MODELS = REPO / "samples/open-world-rpg/assets/models"

PROPS = [
    {
        "name": "crate",
        "src": REPO / "tools/art/crate/Crate.gltf",
        "dst": MODELS / "crate.gltf",
        "png": MODELS / "crate.png",
        "target_height": 1.0,
        "generator": "AI RPG Engine crate bake from Crate.gltf v2-uv-snap",
        "scene_name": "Crate",
        "mesh_name": "Crate",
        "mat_name": "CrateAtlas",
    },
    {
        "name": "bush",
        "src": REPO / "tools/art/bush/Bush.gltf",
        "dst": MODELS / "bush.gltf",
        "png": MODELS / "bush.png",
        "target_height": 1.25,
        "generator": "AI RPG Engine bush bake from Bush.gltf v1",
        "scene_name": "Bush",
        "mesh_name": "Bush",
        "mat_name": "BushAtlas",
    },
    {
        "name": "bush_tall",
        "src": REPO / "tools/art/tall-bush/Tall_Bush.gltf",
        "dst": MODELS / "bush_tall.gltf",
        "png": MODELS / "bush_tall.png",
        "target_height": 1.9,
        "generator": "AI RPG Engine tall bush bake from Tall_Bush.gltf v2-uv-snap",
        "scene_name": "TallBush",
        "mesh_name": "TallBush",
        "mat_name": "TallBushAtlas",
    },
    {
        "name": "campfire",
        "src": REPO / "tools/art/campfire/Campfire_New.gltf",
        "dst": MODELS / "campfire.gltf",
        "png": MODELS / "campfire.png",
        # Stones + cylindrical logs (no flame mesh). ~1.3 m ring. Flip inside-out BB exports.
        "target_height": 0.44,
        "scale_mode": "max_xz",
        "target_span": 1.3,
        "generator": "AI RPG Engine campfire bake from Campfire_New.gltf v3-uv-snap",
        "scene_name": "Campfire",
        "mesh_name": "Campfire",
        "mat_name": "CampfireAtlas",
    },
    {
        "name": "barrel",
        "src": REPO / "tools/art/barrel/Barrel.gltf",
        "dst": MODELS / "barrel.gltf",
        "png": MODELS / "barrel.png",
        "target_height": 1.0,
        "generator": "AI RPG Engine barrel bake from Barrel.gltf v1",
        "scene_name": "Barrel",
        "mesh_name": "Barrel",
        "mat_name": "BarrelAtlas",
    },
    {
        "name": "lantern",
        "src": REPO / "tools/art/lantern/Lantern.gltf",
        "dst": MODELS / "lantern.gltf",
        "png": MODELS / "lantern.png",
        "target_height": 0.55,
        "generator": "AI RPG Engine lantern bake from Lantern.gltf v2-backdrop-clean",
        "max_atlas": 1024,
        "scene_name": "Lantern",
        "mesh_name": "Lantern",
        "mat_name": "LanternAtlas",
    },
    {
        "name": "wall_torch",
        "src": REPO / "tools/art/wall-torch/Wall_Torch.gltf",
        "dst": MODELS / "wall_torch.gltf",
        "png": MODELS / "wall_torch.png",
        "target_height": 0.7,
        "generator": "AI RPG Engine wall torch bake from Wall_Torch.gltf v2-uv-snap",
        "scene_name": "WallTorch",
        "mesh_name": "WallTorch",
        "mat_name": "WallTorchAtlas",
    },
]


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


def load_atlas(g: dict, src: Path) -> Image.Image:
    images = g.get("images") or []
    if not images:
        raise RuntimeError(f"{src}: no images")
    img = images[0]
    uri = img.get("uri", "")
    if uri.startswith("data:image/png;base64,"):
        return Image.open(io.BytesIO(base64.b64decode(uri.split(",", 1)[1]))).convert("RGBA")
    if uri:
        path = (src.parent / uri).resolve()
        return Image.open(path).convert("RGBA")
    raise RuntimeError(f"{src}: unsupported image uri")


def is_blockbench_backdrop(px) -> bool:
    """Near-white / pale UV-editor fills Blockbench embeds behind islands — not prop paint."""
    r, g, b, a = px
    if a < 8:
        return False
    # True near-white padding / holes in island squares
    if r > 210 and g > 210 and b > 210:
        return True
    # Pale cool square fills (e.g. 236,248,253) under circular UV islands
    mx, mn = max(r, g, b), min(r, g, b)
    if mx > 175 and (mx - mn) < 55 and b >= g >= r and (b - r) >= 8:
        return True
    return False


def clean_blockbench_atlas(tex: Image.Image) -> Image.Image:
    """Punch Blockbench backdrop texels to transparent so UV snap lands on real paint."""
    out = tex.convert("RGBA")
    px = out.load()
    w, h = out.size
    cleared = 0
    for y in range(h):
        for x in range(w):
            if is_blockbench_backdrop(px[x, y]):
                px[x, y] = (0, 0, 0, 0)
                cleared += 1
    print(f"  cleared Blockbench backdrop texels: {cleared}")
    return out


def bake_prop(prop: dict) -> None:
    src: Path = prop["src"]
    dst: Path = prop["dst"]
    png_dst: Path = prop["png"]
    target_height: float = prop["target_height"]
    scale_mode = prop.get("scale_mode", "height")
    target_span = float(prop.get("target_span", target_height))

    g = json.loads(src.read_text(encoding="utf-8"))
    tex = load_atlas(g, src)
    if prop.get("clean_backdrop", True):
        tex = clean_blockbench_atlas(tex)
    # Keep pixel-art atlases small for engine point sampling + faster UV snap.
    max_atlas = int(prop.get("max_atlas", 512))
    if max(tex.size) > max_atlas:
        tex = tex.resize((max_atlas, max_atlas), Image.Resampling.NEAREST)
    tw, th = tex.size

    # Precompute nearest opaque texel for every atlas pixel (O(tw*th) once).
    opaque = []
    for y in range(th):
        for x in range(tw):
            if tex.getpixel((x, y))[3] >= 8:
                opaque.append((x, y))
    nearest_opaque = [[None] * tw for _ in range(th)]
    nearest_brown = [[None] * tw for _ in range(th)]

    def is_brown_px(px):
        return px[3] >= 8 and px[0] > px[2] + 10 and px[0] > 50

    if opaque:
        # Seed nearest maps from opaque pixels, then flood-fill for speed.
        from collections import deque

        q = deque()
        qb = deque()
        for x, y in opaque:
            nearest_opaque[y][x] = (x, y)
            q.append((x, y))
            if is_brown_px(tex.getpixel((x, y))):
                nearest_brown[y][x] = (x, y)
                qb.append((x, y))
        while q:
            x, y = q.popleft()
            src_xy = nearest_opaque[y][x]
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                xx, yy = x + dx, y + dy
                if 0 <= xx < tw and 0 <= yy < th and nearest_opaque[yy][xx] is None:
                    nearest_opaque[yy][xx] = src_xy
                    q.append((xx, yy))
        while qb:
            x, y = qb.popleft()
            src_xy = nearest_brown[y][x]
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                xx, yy = x + dx, y + dy
                if 0 <= xx < tw and 0 <= yy < th and nearest_brown[yy][xx] is None:
                    nearest_brown[yy][xx] = src_xy
                    qb.append((xx, yy))

    def resolve_uv(u: float, v: float, prefer_brown: bool = False):
        """Sample atlas; if texel is transparent, snap UV to nearest opaque pixel (prop shader uses albedo tex)."""

        def at_px(x: int, y: int):
            return tex.getpixel((x, y))

        def clamp_uv(uu: float, vv: float):
            x = max(0, min(tw - 1, int(uu * tw)))
            y = max(0, min(th - 1, int(vv * th)))
            return x, y

        x, y = clamp_uv(u, v)
        px = at_px(x, y)
        r, gch, b, a = px
        if a < 8:
            x2, y2 = clamp_uv(u, 1.0 - v)
            px2 = at_px(x2, y2)
            if px2[3] >= 8:
                x, y, px = x2, y2, px2
                u, v = u, 1.0 - v
                r, gch, b, a = px
        if a < 8:
            hit = None
            if prefer_brown and nearest_brown[y][x] is not None:
                hit = nearest_brown[y][x]
            elif nearest_opaque[y][x] is not None:
                hit = nearest_opaque[y][x]
            if hit is not None:
                x, y = hit
                r, gch, b, a = at_px(x, y)
            else:
                r, gch, b, a = (79, 58, 45, 255)
            u = (x + 0.5) / tw
            v = (y + 0.5) / th
        return (r / 255.0, gch / 255.0, b / 255.0, 1.0), (float(u), float(v))

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
    flipped_prims = 0

    for ni, node in enumerate(g["nodes"]):
        if "mesh" not in node or world[ni] is None:
            continue
        mesh = g["meshes"][node["mesh"]]
        m = world[ni]
        prefer_brown = "cylinder" in (node.get("name") or "").lower()
        for prim in mesh["primitives"]:
            attrs = prim["attributes"]
            pos = read_accessor(g, attrs["POSITION"])
            uvs = (
                read_accessor(g, attrs["TEXCOORD_0"])
                if "TEXCOORD_0" in attrs
                else [(0.5, 0.5)] * len(pos)
            )
            idx = list(
                read_accessor(g, prim["indices"]) if "indices" in prim else range(len(pos))
            )
            world_pos = [transform_point(m, p) for p in pos]
            # Keep cylinder/stone geometry; flip winding when Blockbench exports inside-out
            # (engine prop pipeline culls back faces — matches Blockbench shaded view).
            if len(idx) >= 3 and world_pos:
                cx_p = sum(p[0] for p in world_pos) / len(world_pos)
                cy_p = sum(p[1] for p in world_pos) / len(world_pos)
                cz_p = sum(p[2] for p in world_pos) / len(world_pos)
                outward = inward = 0
                for i in range(0, len(idx) - 2, 3):
                    a = world_pos[idx[i]]
                    b = world_pos[idx[i + 1]]
                    c = world_pos[idx[i + 2]]
                    ab = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
                    ac = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
                    nx = ab[1] * ac[2] - ab[2] * ac[1]
                    ny = ab[2] * ac[0] - ab[0] * ac[2]
                    nz = ab[0] * ac[1] - ab[1] * ac[0]
                    cen = (
                        (a[0] + b[0] + c[0]) / 3.0,
                        (a[1] + b[1] + c[1]) / 3.0,
                        (a[2] + b[2] + c[2]) / 3.0,
                    )
                    if nx * (cen[0] - cx_p) + ny * (cen[1] - cy_p) + nz * (cen[2] - cz_p) >= 0.0:
                        outward += 1
                    else:
                        inward += 1
                if inward > outward:
                    for i in range(0, len(idx) - 2, 3):
                        idx[i + 1], idx[i + 2] = idx[i + 2], idx[i + 1]
                    flipped_prims += 1
            base = len(positions)
            for p, uv in zip(world_pos, uvs):
                color, uv_fixed = resolve_uv(float(uv[0]), float(uv[1]), prefer_brown=prefer_brown)
                positions.append(p)
                colors.append(color)
                uv_coords.append(uv_fixed)
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
    span_x = maxx - minx
    span_z = maxz - minz
    if scale_mode == "max_xz":
        span = max(span_x, span_z)
        scale = target_span / span if span > 1e-6 else 1.0
    else:
        scale = target_height / height if height > 1e-6 else 1.0
    print(f"=== {prop['name']} ===")
    print(
        f"raw AABB x[{minx:.4f},{maxx:.4f}] y[{miny:.4f},{maxy:.4f}] "
        f"z[{minz:.4f},{maxz:.4f}] h={height:.4f} spanXZ={max(span_x, span_z):.4f}"
    )
    print(
        f"scale_mode={scale_mode} scale={scale:.4f} center xz=({cx:.4f},{cz:.4f}) "
        f"flipped_prims={flipped_prims}"
    )

    norm_pos = [((x - cx) * scale, (y - miny) * scale, (z - cz) * scale) for x, y, z in positions]

    pos_bytes = pad4(b"".join(struct.pack("<fff", *p) for p in norm_pos))
    col_bytes = pad4(b"".join(struct.pack("<ffff", *c) for c in colors))
    uv_bytes = pad4(b"".join(struct.pack("<ff", *uv) for uv in uv_coords))
    if len(norm_pos) > 65535:
        raise RuntimeError(f"{prop['name']}: too many vertices for UNSIGNED_SHORT indices")
    idx_bytes = pad4(b"".join(struct.pack("<H", i) for i in indices))
    blob = pos_bytes + col_bytes + uv_bytes + idx_bytes
    b64 = base64.b64encode(blob).decode("ascii")

    tex.save(png_dst, format="PNG")

    nverts = len(norm_pos)
    nidx = len(indices)
    nys = [p[1] for p in norm_pos]
    nxs = [p[0] for p in norm_pos]
    nzs = [p[2] for p in norm_pos]
    mesh_name = prop["mesh_name"]
    mat_name = prop["mat_name"]
    png_name = png_dst.name

    out = {
        "asset": {
            "version": "2.0",
            "generator": prop["generator"],
        },
        "scenes": [{"nodes": [0], "name": prop["scene_name"]}],
        "scene": 0,
        "nodes": [{"name": mesh_name, "mesh": 0}],
        "meshes": [
            {
                "name": mesh_name,
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
                "name": mat_name,
                "pbrMetallicRoughness": {
                    "baseColorTexture": {"index": 0},
                    "metallicFactor": 0.0,
                    "roughnessFactor": 1.0,
                },
            }
        ],
        "textures": [{"name": mat_name, "source": 0, "sampler": 0}],
        "images": [{"name": mat_name, "uri": png_name}],
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

    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(json.dumps(out, separators=(",", ":")), encoding="utf-8")
    print(f"wrote {dst} ({dst.stat().st_size} bytes) verts={nverts} tris={nidx // 3}")
    print(f"wrote {png_dst} ({png_dst.stat().st_size} bytes) atlas={tw}x{th}")
    print(
        f"final AABB x[{min(nxs):.3f},{max(nxs):.3f}] y[{min(nys):.3f},{max(nys):.3f}] "
        f"z[{min(nzs):.3f},{max(nzs):.3f}]"
    )


def main() -> None:
    import sys

    names = set(sys.argv[1:])
    for prop in PROPS:
        if names and prop["name"] not in names:
            continue
        bake_prop(prop)


if __name__ == "__main__":
    main()
