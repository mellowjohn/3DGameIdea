"""Bake Blockbench Tree.bbmodel into an engine-ready static mesh.

Converts free-form mesh elements (tris/quads) + atlas into a flattened glTF with
TEXCOORD_0, baseColorTexture (tree.png), and COLOR_0 fallback. Feet at y=0,
height normalized to TARGET_HEIGHT meters.
"""

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
DEFAULT_SRC = REPO / "tools/art/tree/Tree.bbmodel"
DEFAULT_DST = REPO / "samples/open-world-rpg/assets/models/tree.gltf"
DEFAULT_PNG = DEFAULT_DST.parent / "tree.png"
DEFAULT_HEIGHT = 3.0


def pad4(b: bytes) -> bytes:
    return b + (b"\x00" * ((4 - (len(b) % 4)) % 4))


def euler_xyz_deg_to_mat(rx: float, ry: float, rz: float):
    """Blockbench element rotation: XYZ Euler in degrees → 4x4 row-major."""
    ax, ay, az = map(math.radians, (rx, ry, rz))
    cx, sx = math.cos(ax), math.sin(ax)
    cy, sy = math.cos(ay), math.sin(ay)
    cz, sz = math.cos(az), math.sin(az)
    # R = Rz * Ry * Rx
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
    return (r / 255.0, g / 255.0, b / 255.0, max(a / 255.0, 1.0))


def bake_bbmodel(
    src: Path,
    dst: Path,
    *,
    atlas_png: Path | None = None,
    write_atlas: bool = True,
    atlas_uri: str = "tree.png",
    target_height: float = DEFAULT_HEIGHT,
    mesh_name: str = "Tree",
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
        if element.get("type") != "mesh":
            continue
        verts = element.get("vertices") or {}
        faces = element.get("faces") or {}
        origin = element.get("origin") or [0.0, 0.0, 0.0]
        rotation = element.get("rotation") or [0.0, 0.0, 0.0]
        rot = euler_xyz_deg_to_mat(rotation[0], rotation[1], rotation[2])

        for face in faces.values():
            if face.get("texture") is None:
                continue
            vids = face.get("vertices") or []
            if len(vids) < 3:
                continue
            uvs = face.get("uv") or {}
            for a, b, c in triangulate(vids):
                tri = []
                for vid in (a, b, c):
                    if vid not in verts:
                        raise RuntimeError(f"missing vertex {vid} in {element.get('name')}")
                    local = verts[vid]
                    world = transform_point(rot, local, origin)
                    uv_px = uvs.get(vid) or [0.0, 0.0]
                    u = float(uv_px[0]) / tw_res
                    v = float(uv_px[1]) / th_res
                    positions.append(world)
                    uv_coords.append((u, v))
                    colors.append(sample_uv(tex, u, v))
                    tri.append(len(positions) - 1)
                indices.extend(tri)

    if not positions:
        raise RuntimeError(f"no mesh geometry exported from {src.name}")

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
        f"z[{minz:.4f},{maxz:.4f}] h={height:.4f}"
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
            "generator": f"AI RPG Engine tree bake from {src.name}",
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
                "name": "TreeAtlas",
                "pbrMetallicRoughness": {
                    "baseColorTexture": {"index": 0},
                    "metallicFactor": 0.0,
                    "roughnessFactor": 1.0,
                },
            }
        ],
        "textures": [{"name": "TreeAtlas", "source": 0, "sampler": 0}],
        "images": [{"name": "TreeAtlas", "uri": atlas_uri}],
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
    parser = argparse.ArgumentParser(description="Bake Blockbench tree bbmodel to engine glTF")
    parser.add_argument("--src", type=Path, default=DEFAULT_SRC)
    parser.add_argument("--dst", type=Path, default=DEFAULT_DST)
    parser.add_argument("--atlas", type=Path, default=DEFAULT_PNG)
    parser.add_argument("--atlas-uri", default="tree.png")
    parser.add_argument("--height", type=float, default=DEFAULT_HEIGHT)
    parser.add_argument("--name", default="Tree")
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
