"""Bake Loot_Bag.bbmodel (mesh + cube elements) into an engine-ready static glTF."""

from __future__ import annotations

import argparse
import base64
import io
import json
import math
import struct
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[1]
DEFAULT_SRC = REPO / "tools/art/loot-bag/Loot_Bag.bbmodel"
DEFAULT_DST = REPO / "samples/open-world-rpg/assets/models/loot_bag.gltf"
DEFAULT_PNG = DEFAULT_DST.parent / "loot_bag.png"
DEFAULT_HEIGHT = 0.45


def pad4(b: bytes) -> bytes:
    return b + (b"\x00" * ((4 - (len(b) % 4)) % 4))


def euler_xyz_deg_to_mat(rx: float, ry: float, rz: float):
    ax, ay, az = map(math.radians, (rx, ry, rz))
    cx, sx = math.cos(ax), math.sin(ax)
    cy, sy = math.cos(ay), math.sin(ay)
    cz, sz = math.cos(az), math.sin(az)
    return [
        [cy * cz, sx * sy * cz - cx * sz, cx * sy * cz + sx * sz, 0.0],
        [cy * sz, sx * sy * sz + cx * cz, cx * sy * sz - sx * cz, 0.0],
        [-sy, sx * cy, cx * cy, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]


def transform_point(m, p, origin):
    x, y, z = p
    wx = m[0][0] * x + m[0][1] * y + m[0][2] * z + origin[0]
    wy = m[1][0] * x + m[1][1] * y + m[1][2] * z + origin[1]
    wz = m[2][0] * x + m[2][1] * y + m[2][2] * z + origin[2]
    return (wx, wy, wz)


def triangulate(indices: list[str]) -> list[tuple[str, str, str]]:
    if len(indices) == 3:
        return [(indices[0], indices[1], indices[2])]
    if len(indices) == 4:
        return [(indices[0], indices[1], indices[2]), (indices[0], indices[2], indices[3])]
    out = []
    for i in range(1, len(indices) - 1):
        out.append((indices[0], indices[i], indices[i + 1]))
    return out


def sample_uv(tex: Image.Image, u: float, v: float):
    tw, th = tex.size

    def at(uu: float, vv: float):
        x = max(0, min(tw - 1, int(uu * tw)))
        y = max(0, min(th - 1, int(vv * th)))
        return tex.getpixel((x, y))

    r, g, b, a = at(u, v)
    if a < 8:
        r, g, b, a = at(u, 1.0 - v)
    if a < 8:
        r, g, b, a = (92, 70, 54, 255)
    return (r / 255.0, g / 255.0, b / 255.0, 1.0)


def cube_local_corners(frm, to):
    x0, y0, z0 = frm
    x1, y1, z1 = to
    return {
        "000": [x0, y0, z0],
        "001": [x0, y0, z1],
        "010": [x0, y1, z0],
        "011": [x0, y1, z1],
        "100": [x1, y0, z0],
        "101": [x1, y0, z1],
        "110": [x1, y1, z0],
        "111": [x1, y1, z1],
    }


# Blockbench cube face vertex order (outward) + UV corners [u0,v0,u1,v1] → 4 verts
CUBE_FACES = {
    "north": ("001", "000", "010", "011"),
    "south": ("100", "101", "111", "110"),
    "west": ("000", "100", "110", "010"),
    "east": ("101", "001", "011", "111"),
    "up": ("010", "110", "111", "011"),
    "down": ("000", "001", "101", "100"),
}


def emit_tri(
    positions,
    colors,
    uv_coords,
    indices,
    pts,
    uvs,
    tex,
    tw_res,
    th_res,
):
    for a, b, c in ((0, 1, 2), (0, 2, 3)) if len(pts) == 4 else ((0, 1, 2),):
        for i in (a, b, c):
            positions.append(pts[i])
            u, v = uvs[i]
            uv_coords.append((u, v))
            colors.append(sample_uv(tex, u, v))
            indices.append(len(positions) - 1)


def bake_bbmodel(
    src: Path,
    dst: Path,
    *,
    atlas_png: Path | None = None,
    write_atlas: bool = True,
    atlas_uri: str = "loot_bag.png",
    target_height: float = DEFAULT_HEIGHT,
    mesh_name: str = "LootBag",
) -> None:
    model = json.loads(src.read_text(encoding="utf-8"))
    res = model.get("resolution") or {"width": 16, "height": 16}
    tw_res = float(res["width"])
    th_res = float(res["height"])

    textures = model.get("textures") or []
    if not textures:
        raise RuntimeError(f"{src.name} has no textures")
    src_uri = textures[0].get("source") or ""
    if not src_uri.startswith("data:image/png;base64,"):
        raise RuntimeError("expected embedded PNG texture source")
    tex = Image.open(io.BytesIO(base64.b64decode(src_uri.split(",", 1)[1]))).convert("RGBA")

    positions: list[tuple[float, float, float]] = []
    colors: list[tuple[float, float, float, float]] = []
    uv_coords: list[tuple[float, float]] = []
    indices: list[int] = []

    for element in model.get("elements") or []:
        if element.get("visibility") is False or element.get("export") is False:
            continue
        et = element.get("type") or "cube"
        origin = element.get("origin") or [0.0, 0.0, 0.0]
        rotation = element.get("rotation") or [0.0, 0.0, 0.0]
        rot = euler_xyz_deg_to_mat(rotation[0], rotation[1], rotation[2])

        if et == "mesh":
            verts = element.get("vertices") or {}
            faces = element.get("faces") or {}
            for face in faces.values():
                if face.get("texture") is None:
                    continue
                vids = face.get("vertices") or []
                if len(vids) < 3:
                    continue
                uvs = face.get("uv") or {}
                for a, b, c in triangulate(vids):
                    tri_pts = []
                    tri_uvs = []
                    for vid in (a, b, c):
                        local = verts[vid]
                        # Mesh verts are already in element space; origin is pivot.
                        world = transform_point(rot, [local[0], local[1], local[2]], origin)
                        uv_px = uvs.get(vid) or [0.0, 0.0]
                        tri_pts.append(world)
                        tri_uvs.append((float(uv_px[0]) / tw_res, float(uv_px[1]) / th_res))
                    emit_tri(positions, colors, uv_coords, indices, tri_pts, tri_uvs, tex, tw_res, th_res)
            continue

        if et != "cube":
            continue
        frm = element.get("from") or [0, 0, 0]
        to = element.get("to") or [1, 1, 1]
        corners = cube_local_corners(frm, to)
        # Cube coords are absolute in BB free format; subtract origin then rotate.
        faces = element.get("faces") or {}
        for fname, order in CUBE_FACES.items():
            face = faces.get(fname)
            if not face or face.get("texture") is None:
                continue
            uv = face.get("uv") or [0, 0, 1, 1]
            u0, v0, u1, v1 = [float(x) for x in uv]
            uv_corners = [
                (u0 / tw_res, v1 / th_res),
                (u1 / tw_res, v1 / th_res),
                (u1 / tw_res, v0 / th_res),
                (u0 / tw_res, v0 / th_res),
            ]
            pts = []
            for key in order:
                lx, ly, lz = corners[key]
                local = [lx - origin[0], ly - origin[1], lz - origin[2]]
                pts.append(transform_point(rot, local, origin))
            emit_tri(positions, colors, uv_coords, indices, pts, uv_corners, tex, tw_res, th_res)

    if not positions:
        raise RuntimeError(f"no geometry exported from {src.name}")

    # Flip majority-inward triangles (engine backface cull).
    flipped = 0
    cx0 = sum(p[0] for p in positions) / len(positions)
    cy0 = sum(p[1] for p in positions) / len(positions)
    cz0 = sum(p[2] for p in positions) / len(positions)
    for i in range(0, len(indices), 3):
        i0, i1, i2 = indices[i], indices[i + 1], indices[i + 2]
        a, b, c = positions[i0], positions[i1], positions[i2]
        e1 = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
        e2 = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
        nx = e1[1] * e2[2] - e1[2] * e2[1]
        ny = e1[2] * e2[0] - e1[0] * e2[2]
        nz = e1[0] * e2[1] - e1[1] * e2[0]
        mx = (a[0] + b[0] + c[0]) / 3.0 - cx0
        my = (a[1] + b[1] + c[1]) / 3.0 - cy0
        mz = (a[2] + b[2] + c[2]) / 3.0 - cz0
        if nx * mx + ny * my + nz * mz < 0:
            indices[i + 1], indices[i + 2] = i2, i1
            flipped += 1

    xs = [p[0] for p in positions]
    ys = [p[1] for p in positions]
    zs = [p[2] for p in positions]
    minx, maxx = min(xs), max(xs)
    miny, maxy = min(ys), max(ys)
    minz, maxz = min(zs), max(zs)
    cx = 0.5 * (minx + maxx)
    cz = 0.5 * (minz + maxz)
    height = maxy - miny
    scale = target_height / height if height > 1e-6 else 1.0
    print(
        f"[{mesh_name}] raw AABB x[{minx:.4f},{maxx:.4f}] y[{miny:.4f},{maxy:.4f}] "
        f"z[{minz:.4f},{maxz:.4f}] h={height:.4f} flipped={flipped}"
    )
    print(f"[{mesh_name}] scale={scale:.6f} center xz=({cx:.4f},{cz:.4f})")

    norm_pos = [((x - cx) * scale, (y - miny) * scale, (z - cz) * scale) for x, y, z in positions]

    pos_bytes = pad4(b"".join(struct.pack("<fff", *p) for p in norm_pos))
    col_bytes = pad4(b"".join(struct.pack("<ffff", *c) for c in colors))
    uv_bytes = pad4(b"".join(struct.pack("<ff", *uv) for uv in uv_coords))
    if len(norm_pos) > 65535:
        raise RuntimeError("too many vertices for UNSIGNED_SHORT indices")
    idx_bytes = pad4(b"".join(struct.pack("<H", i) for i in indices))
    blob = pos_bytes + col_bytes + uv_bytes + idx_bytes
    b64 = base64.b64encode(blob).decode("ascii")

    png_path = atlas_png or (dst.parent / atlas_uri)
    if write_atlas:
        tex.save(png_path, format="PNG")

    nverts = len(norm_pos)
    nidx = len(indices)
    nys = [p[1] for p in norm_pos]
    nxs = [p[0] for p in norm_pos]
    nzs = [p[2] for p in norm_pos]

    out = {
        "asset": {
            "version": "2.0",
            "generator": "AI RPG Engine loot bag bake from Loot_Bag.bbmodel v1",
        },
        "scenes": [{"nodes": [0], "name": mesh_name}],
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
                "name": "LootBagAtlas",
                "pbrMetallicRoughness": {
                    "baseColorTexture": {"index": 0},
                    "metallicFactor": 0.0,
                    "roughnessFactor": 1.0,
                },
            }
        ],
        "textures": [{"name": "LootBagAtlas", "source": 0, "sampler": 0}],
        "images": [{"name": "LootBagAtlas", "uri": atlas_uri}],
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
    if write_atlas:
        print(f"wrote {png_path} ({png_path.stat().st_size} bytes) atlas={tex.size[0]}x{tex.size[1]}")
    print(
        f"[{mesh_name}] final AABB x[{min(nxs):.3f},{max(nxs):.3f}] y[{min(nys):.3f},{max(nys):.3f}] "
        f"z[{min(nzs):.3f},{max(nzs):.3f}]"
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Bake loot bag bbmodel to engine glTF")
    parser.add_argument("--src", type=Path, default=DEFAULT_SRC)
    parser.add_argument("--dst", type=Path, default=DEFAULT_DST)
    parser.add_argument("--atlas", type=Path, default=DEFAULT_PNG)
    parser.add_argument("--atlas-uri", default="loot_bag.png")
    parser.add_argument("--height", type=float, default=DEFAULT_HEIGHT)
    parser.add_argument("--name", default="LootBag")
    parser.add_argument("--no-write-atlas", action="store_true")
    args = parser.parse_args()
    bake_bbmodel(
        args.src,
        args.dst,
        atlas_png=args.atlas,
        write_atlas=not args.no_write_atlas,
        atlas_uri=args.atlas_uri,
        target_height=args.height,
        mesh_name=args.name,
    )


if __name__ == "__main__":
    main()
