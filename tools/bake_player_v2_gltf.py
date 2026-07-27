"""Bake Player_V2_rigged.gltf into engine-ready player.gltf (skinned + Idle).

Preserves skins, JOINTS_0/WEIGHTS_0, and animations. Cleans Blockbench marker
colors from the atlas, pads UV islands, normalizes feet to y=0 / ~2.75 m tall.
"""
from __future__ import annotations

import base64
import io
import json
import shutil
import struct
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parents[1]
# Prefer a fresh Blockbench glTF re-export when present.
SRC_CANDIDATES = [
    Path(r"c:\Users\johnr\Documents\Models\Player_V2_rigged.gltf"),
    Path(r"c:\Users\johnr\Documents\Player_V2_rigged.gltf"),
    REPO / "tools/art/player/Player_V2_rigged.gltf",
]
# Prefer the authored atlas when present (cleaner than re-extracting after a bad pad).
SRC_PNG_CANDIDATES = [
    Path(r"c:\Users\johnr\Documents\Player_V2.png"),
    REPO / "tools/art/player/Player_V2.png",
]
DST = REPO / "samples/open-world-rpg/assets/models/player.gltf"
PNG_DST = DST.with_name("player.png")
ART_DIR = REPO / "tools/art/player"
TARGET_HEIGHT = 2.75


def first_existing(paths: list[Path]) -> Path | None:
    for path in paths:
        if path.exists():
            return path
    return None


def read_blob(g: dict) -> bytearray:
    uri = g["buffers"][0]["uri"]
    assert uri.startswith("data:application/octet-stream;base64,")
    return bytearray(base64.b64decode(uri.split(",", 1)[1]))


def write_blob(g: dict, raw: bytes) -> None:
    g["buffers"][0]["uri"] = (
        "data:application/octet-stream;base64," + base64.b64encode(raw).decode("ascii")
    )
    g["buffers"][0]["byteLength"] = len(raw)


def accessor_meta(g: dict, acc_idx: int):
    acc = g["accessors"][acc_idx]
    bv = g["bufferViews"][acc["bufferView"]]
    offset = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    ctype = acc["componentType"]
    typ = acc["type"]
    comps = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[typ]
    size = {5126: 4, 5123: 2, 5121: 1, 5125: 4}[ctype]
    stride = bv.get("byteStride", size * comps)
    return acc, offset, comps, size, stride, ctype


def read_accessor(g: dict, raw: bytes, acc_idx: int):
    acc, offset, comps, size, stride, ctype = accessor_meta(g, acc_idx)
    fmt = {5126: "f", 5123: "H", 5121: "B", 5125: "I"}[ctype]
    out = []
    for i in range(acc["count"]):
        o = offset + i * stride
        vals = struct.unpack_from("<" + fmt * comps, raw, o)
        out.append(vals if comps > 1 else vals[0])
    return out


def write_f32_accessor(g: dict, raw: bytearray, acc_idx: int, values) -> None:
    acc, offset, comps, size, stride, ctype = accessor_meta(g, acc_idx)
    assert ctype == 5126
    for i, val in enumerate(values):
        o = offset + i * stride
        if comps == 1:
            struct.pack_into("<f", raw, o, float(val))
        else:
            struct.pack_into("<" + "f" * comps, raw, o, *[float(x) for x in val])
    if acc.get("type") == "VEC3" and values:
        xs = [v[0] for v in values]
        ys = [v[1] for v in values]
        zs = [v[2] for v in values]
        acc["min"] = [min(xs), min(ys), min(zs)]
        acc["max"] = [max(xs), max(ys), max(zs)]


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


def translate_matrix(tx, ty, tz):
    return [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, tx, ty, tz, 1]


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


def scale_diag(sx, sy, sz):
    return [sx, 0, 0, 0, 0, sy, 0, 0, 0, 0, sz, 0, 0, 0, 0, 1]


def node_local_matrix(node: dict):
    t = node.get("translation", [0.0, 0.0, 0.0])
    r = node.get("rotation", [0.0, 0.0, 0.0, 1.0])
    s = node.get("scale", [1.0, 1.0, 1.0])
    return mat4_mul(translate_matrix(*t), mat4_mul(quat_matrix(r), scale_diag(*s)))


def mat4_invert(m):
    """Invert a column-major 4x4 (affine TRS is enough for bind poses)."""
    # General 4x4 inverse via cofactors.
    inv = [0.0] * 16
    inv[0] = (
        m[5] * m[10] * m[15]
        - m[5] * m[11] * m[14]
        - m[9] * m[6] * m[15]
        + m[9] * m[7] * m[14]
        + m[13] * m[6] * m[11]
        - m[13] * m[7] * m[10]
    )
    inv[4] = (
        -m[4] * m[10] * m[15]
        + m[4] * m[11] * m[14]
        + m[8] * m[6] * m[15]
        - m[8] * m[7] * m[14]
        - m[12] * m[6] * m[11]
        + m[12] * m[7] * m[10]
    )
    inv[8] = (
        m[4] * m[9] * m[15]
        - m[4] * m[11] * m[13]
        - m[8] * m[5] * m[15]
        + m[8] * m[7] * m[13]
        + m[12] * m[5] * m[11]
        - m[12] * m[7] * m[9]
    )
    inv[12] = (
        -m[4] * m[9] * m[14]
        + m[4] * m[10] * m[13]
        + m[8] * m[5] * m[14]
        - m[8] * m[6] * m[13]
        - m[12] * m[5] * m[10]
        + m[12] * m[6] * m[9]
    )
    inv[1] = (
        -m[1] * m[10] * m[15]
        + m[1] * m[11] * m[14]
        + m[9] * m[2] * m[15]
        - m[9] * m[3] * m[14]
        - m[13] * m[2] * m[11]
        + m[13] * m[3] * m[10]
    )
    inv[5] = (
        m[0] * m[10] * m[15]
        - m[0] * m[11] * m[14]
        - m[8] * m[2] * m[15]
        + m[8] * m[3] * m[14]
        + m[12] * m[2] * m[11]
        - m[12] * m[3] * m[10]
    )
    inv[9] = (
        -m[0] * m[9] * m[15]
        + m[0] * m[11] * m[13]
        + m[8] * m[1] * m[15]
        - m[8] * m[3] * m[13]
        - m[12] * m[1] * m[11]
        + m[12] * m[3] * m[9]
    )
    inv[13] = (
        m[0] * m[9] * m[14]
        - m[0] * m[10] * m[13]
        - m[8] * m[1] * m[14]
        + m[8] * m[2] * m[13]
        + m[12] * m[1] * m[10]
        - m[12] * m[2] * m[9]
    )
    inv[2] = (
        m[1] * m[6] * m[15]
        - m[1] * m[7] * m[14]
        - m[5] * m[2] * m[15]
        + m[5] * m[3] * m[14]
        + m[13] * m[2] * m[7]
        - m[13] * m[3] * m[6]
    )
    inv[6] = (
        -m[0] * m[6] * m[15]
        + m[0] * m[7] * m[14]
        + m[4] * m[2] * m[15]
        - m[4] * m[3] * m[14]
        - m[12] * m[2] * m[7]
        + m[12] * m[3] * m[6]
    )
    inv[10] = (
        m[0] * m[5] * m[15]
        - m[0] * m[7] * m[13]
        - m[4] * m[1] * m[15]
        + m[4] * m[3] * m[13]
        + m[12] * m[1] * m[7]
        - m[12] * m[3] * m[5]
    )
    inv[14] = (
        -m[0] * m[5] * m[14]
        + m[0] * m[6] * m[13]
        + m[4] * m[1] * m[14]
        - m[4] * m[2] * m[13]
        - m[12] * m[1] * m[6]
        + m[12] * m[2] * m[5]
    )
    inv[3] = (
        -m[1] * m[6] * m[11]
        + m[1] * m[7] * m[10]
        + m[5] * m[2] * m[11]
        - m[5] * m[3] * m[10]
        - m[9] * m[2] * m[7]
        + m[9] * m[3] * m[6]
    )
    inv[7] = (
        m[0] * m[6] * m[11]
        - m[0] * m[7] * m[10]
        - m[4] * m[2] * m[11]
        + m[4] * m[3] * m[10]
        + m[8] * m[2] * m[7]
        - m[8] * m[3] * m[6]
    )
    inv[11] = (
        -m[0] * m[5] * m[11]
        + m[0] * m[7] * m[9]
        + m[4] * m[1] * m[11]
        - m[4] * m[3] * m[9]
        - m[8] * m[1] * m[7]
        + m[8] * m[3] * m[5]
    )
    inv[15] = (
        m[0] * m[5] * m[10]
        - m[0] * m[6] * m[9]
        - m[4] * m[1] * m[10]
        + m[4] * m[2] * m[9]
        + m[8] * m[1] * m[6]
        - m[8] * m[2] * m[5]
    )
    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12]
    if abs(det) < 1e-12:
        raise ValueError("singular matrix")
    inv_det = 1.0 / det
    return [v * inv_det for v in inv]


def joint_world_matrices(g: dict):
    nodes = g["nodes"]
    parent = {i: None for i in range(len(nodes))}
    for i, node in enumerate(nodes):
        for child in node.get("children", []):
            parent[child] = i
    cache = {}

    def world(i: int):
        if i in cache:
            return cache[i]
        local = node_local_matrix(nodes[i])
        p = parent[i]
        cache[i] = local if p is None else mat4_mul(world(p), local)
        return cache[i]

    return [world(i) for i in range(len(nodes))]


def is_transparent(c) -> bool:
    return c[3] < 8


def is_flat_black(c) -> bool:
    """Atlas backdrop / UV gutter (not warm dark cloth)."""
    r, g, b, a = c
    return a < 8 or max(r, g, b) < 28


def is_content(c) -> bool:
    """Skin, warm cloth, pupils — not Blockbench markers or UV gutters."""
    r, g, b, a = c
    if a < 8:
        return False
    mx, mn = max(r, g, b), min(r, g, b)
    # Pure black backdrop is not content (cleared before pad).
    if mx < 28:
        return False
    # Warm dark cloth (shorts) — brown-black, not pure gutter
    if mx < 90 and r >= g >= b and (r - b) >= 8:
        return True
    # Near-white is Blockbench UV island padding / overlay — not sclera on this atlas.
    if r > 200 and g > 200 and b > 200:
        return False
    # Warm skin / leather (R dominant over B, not neon-flat)
    if r >= g >= b and (r - g) >= 25 and (r - b) >= 50:
        return True
    # Dark brown shorts (low-mid, warm)
    if r > g >= b and mx < 160 and (r - b) >= 20 and (mx - mn) < 90:
        return True
    # Reject high-value high-chroma marker paints (yellow/pink/cyan/lime)
    if mx >= 200 and (mx - mn) >= 40:
        return False
    # Soft mid tones that aren't neon
    if mx < 200 and (mx - mn) < 100:
        return True
    return False


def is_marker(c) -> bool:
    return (not is_transparent(c)) and (not is_flat_black(c)) and (not is_content(c))


def clear_edge_background(img: Image.Image) -> Image.Image:
    """Flood-fill flat black/transparent from the atlas edges → alpha 0.

    Keeps interior black pupils (not edge-connected). Warm dark cloth is not flat black.
    """
    from collections import deque

    img = img.convert("RGBA")
    w, h = img.size
    out = img.copy()
    px = out.load()
    seen = [[False] * w for _ in range(h)]
    q: deque[tuple[int, int]] = deque()

    def try_seed(x: int, y: int) -> None:
        if seen[y][x] or not is_flat_black(px[x, y]):
            return
        seen[y][x] = True
        q.append((x, y))

    for x in range(w):
        try_seed(x, 0)
        try_seed(x, h - 1)
    for y in range(h):
        try_seed(0, y)
        try_seed(w - 1, y)

    cleared = 0
    while q:
        x, y = q.popleft()
        px[x, y] = (0, 0, 0, 0)
        cleared += 1
        for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= nx < w and 0 <= ny < h and not seen[ny][nx] and is_flat_black(px[nx, ny]):
                seen[ny][nx] = True
                q.append((nx, ny))
    print(f"  cleared edge backdrop pixels: {cleared}")
    return out


def clean_atlas(img: Image.Image, pad_radius: int = 2) -> Image.Image:
    img = clear_edge_background(img.convert("RGBA"))
    w, h = img.size
    src = img.copy()
    sp = src.load()
    out = img.copy()
    op = out.load()

    # Replace marker/overlay pixels with nearest content color
    replaced = 0
    for y in range(h):
        for x in range(w):
            if not is_marker(sp[x, y]):
                continue
            best = None
            best_d = 1e9
            for rad in range(1, 8):
                for dy in range(-rad, rad + 1):
                    for dx in range(-rad, rad + 1):
                        if abs(dx) != rad and abs(dy) != rad:
                            continue
                        nx, ny = x + dx, y + dy
                        if not (0 <= nx < w and 0 <= ny < h):
                            continue
                        c = sp[nx, ny]
                        if not is_content(c):
                            continue
                        d = dx * dx + dy * dy
                        if d < best_d:
                            best_d, best = d, c
                if best is not None:
                    break
            if best is not None:
                op[x, y] = best
                replaced += 1
            else:
                op[x, y] = (0, 0, 0, 0)
    print(f"  replaced marker pixels: {replaced}")

    # Dilate content into transparent gutters so triangle edges don't sample black.
    cur = out.copy()
    for _ in range(pad_radius):
        nxt = cur.copy()
        cp, npx = cur.load(), nxt.load()
        for y in range(h):
            for x in range(w):
                if not is_transparent(cp[x, y]):
                    continue
                for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and 0 <= ny < h and is_content(cp[nx, ny]):
                        r, g, b, _a = cp[nx, ny]
                        npx[x, y] = (r, g, b, 255)
                        break
        cur = nxt
    return cur


def ensure_color0(g: dict, raw: bytearray, prim: dict, tex: Image.Image) -> bytearray:
    """Bake COLOR_0 from atlas as a fallback tint matching UVs."""
    if "TEXCOORD_0" not in prim["attributes"]:
        return raw
    uvs = read_accessor(g, raw, prim["attributes"]["TEXCOORD_0"])
    tw, th = tex.size
    px = tex.load()
    colors = []
    for u, v in uvs:
        # Engine/D3D samples V as-is (top-left). Match export_player_v2_bbmodel_gltf.py.
        x = max(0, min(tw - 1, int(u * tw)))
        y = max(0, min(th - 1, int(v * th)))
        cr, cg, cb, ca = px[x, y]
        if ca < 8:
            # Nearest opaque texel (UV island edge bleed)
            best = (cr, cg, cb, ca)
            best_d = 1e9
            for rad in range(1, 6):
                for dy in range(-rad, rad + 1):
                    for dx in range(-rad, rad + 1):
                        if abs(dx) != rad and abs(dy) != rad:
                            continue
                        nx, ny = x + dx, y + dy
                        if not (0 <= nx < tw and 0 <= ny < th):
                            continue
                        pr, pg, pb, pa = px[nx, ny]
                        if pa < 8:
                            continue
                        d = dx * dx + dy * dy
                        if d < best_d:
                            best_d, best = d, (pr, pg, pb, pa)
                if best[3] >= 8:
                    break
            cr, cg, cb, ca = best
        if ca < 8:
            cr, cg, cb = 200, 150, 110
        colors.append((cr / 255.0, cg / 255.0, cb / 255.0, 1.0))

    # Append COLOR_0 buffer data
    col_bytes = b"".join(struct.pack("<ffff", *c) for c in colors)
    pad = (4 - (len(col_bytes) % 4)) % 4
    col_bytes += b"\x00" * pad
    offset = len(raw)
    raw.extend(col_bytes)
    view_idx = len(g["bufferViews"])
    g["bufferViews"].append(
        {"buffer": 0, "byteOffset": offset, "byteLength": len(col_bytes) - pad}
    )
    acc_idx = len(g["accessors"])
    g["accessors"].append(
        {
            "bufferView": view_idx,
            "componentType": 5126,
            "count": len(colors),
            "type": "VEC4",
        }
    )
    prim["attributes"]["COLOR_0"] = acc_idx
    g["buffers"][0]["byteLength"] = len(raw)
    return raw


def ensure_idle_animation(g: dict, raw: bytearray) -> bytearray:
    """Blockbench re-exports often drop clips; restore Idle from art or a prior bake."""
    if any(a.get("name") == "Idle" for a in g.get("animations", [])):
        return raw
    donors = [
        ART_DIR / "Player_V2_rigged.gltf",
        DST,
        REPO / "samples/open-world-rpg/assets/models/player.gltf",
    ]
    idle = None
    donor = None
    donor_path = None
    donor_is_baked = False
    for path in donors:
        if not path.exists():
            continue
        candidate = json.loads(path.read_text(encoding="utf-8"))
        clip = next((a for a in candidate.get("animations", []) if a.get("name") == "Idle"), None)
        if clip is None:
            continue
        gen = (candidate.get("asset") or {}).get("generator", "")
        baked = "bake_player_v2_gltf" in gen
        if not baked:
            donor_path, donor, idle, donor_is_baked = path, candidate, clip, False
            break
        if idle is None:
            donor_path, donor, idle, donor_is_baked = path, candidate, clip, True
    if idle is None:
        print("WARNING: no Idle donor found; animator idle will be missing")
        return raw

    donor_by_name = {n.get("name"): i for i, n in enumerate(donor["nodes"]) if n.get("name")}
    dest_by_name = {n.get("name"): i for i, n in enumerate(g["nodes"]) if n.get("name")}

    unscale = 1.0
    if donor_is_baked:
        dest_pos = read_accessor(g, raw, g["meshes"][0]["primitives"][0]["attributes"]["POSITION"])
        src_h = max(p[1] for p in dest_pos) - min(p[1] for p in dest_pos)
        if src_h > 1e-6:
            unscale = src_h / TARGET_HEIGHT
            print(f"Unscaling Idle translations by {unscale:.6f} (from baked donor)")

    donor_raw = read_blob(donor)
    new_anim = {"name": "Idle", "samplers": [], "channels": []}
    channel_paths = {ch["sampler"]: ch.get("target", {}).get("path") for ch in idle["channels"]}
    for si, sampler in enumerate(idle["samplers"]):
        new_sampler = {"interpolation": sampler.get("interpolation", "LINEAR")}
        for key in ("input", "output"):
            old_acc = donor["accessors"][sampler[key]]
            old_bv = donor["bufferViews"][old_acc["bufferView"]]
            start = old_bv.get("byteOffset", 0)
            chunk = bytearray(donor_raw[start : start + old_bv["byteLength"]])
            if (
                key == "output"
                and donor_is_baked
                and channel_paths.get(si) == "translation"
                and abs(unscale - 1.0) > 1e-8
            ):
                acc_off = old_acc.get("byteOffset", 0)
                stride = old_bv.get("byteStride", 12)
                for i in range(old_acc["count"]):
                    o = acc_off + i * stride
                    x, y, z = struct.unpack_from("<fff", chunk, o)
                    struct.pack_into("<fff", chunk, o, x * unscale, y * unscale, z * unscale)
            pad = (4 - (len(raw) % 4)) % 4
            raw.extend(b"\x00" * pad)
            off = len(raw)
            raw.extend(chunk)
            view_idx = len(g["bufferViews"])
            view = {"buffer": 0, "byteOffset": off, "byteLength": len(chunk)}
            if "byteStride" in old_bv:
                view["byteStride"] = old_bv["byteStride"]
            g["bufferViews"].append(view)
            acc = {
                "bufferView": view_idx,
                "componentType": old_acc["componentType"],
                "count": old_acc["count"],
                "type": old_acc["type"],
            }
            if "byteOffset" in old_acc:
                acc["byteOffset"] = old_acc["byteOffset"]
            if "min" in old_acc:
                mn = list(old_acc["min"])
                if key == "output" and donor_is_baked and channel_paths.get(si) == "translation":
                    mn = [v * unscale for v in mn]
                acc["min"] = mn
            if "max" in old_acc:
                mx = list(old_acc["max"])
                if key == "output" and donor_is_baked and channel_paths.get(si) == "translation":
                    mx = [v * unscale for v in mx]
                acc["max"] = mx
            acc_idx = len(g["accessors"])
            g["accessors"].append(acc)
            new_sampler[key] = acc_idx
        new_anim["samplers"].append(new_sampler)

    skipped = 0
    for ch in idle["channels"]:
        src_node = ch["target"]["node"]
        src_name = donor["nodes"][src_node].get("name")
        if src_name not in dest_by_name:
            skipped += 1
            continue
        new_anim["channels"].append(
            {
                "sampler": ch["sampler"],
                "target": {"node": dest_by_name[src_name], "path": ch["target"]["path"]},
            }
        )
    if not new_anim["channels"]:
        print("WARNING: Idle channels could not be remapped; skipping")
        return raw

    g.setdefault("animations", []).append(new_anim)
    g["buffers"][0]["byteLength"] = len(raw)
    print(
        f"Restored Idle ({len(new_anim['channels'])} channels) from {donor_path.name}"
        + (f" (skipped {skipped})" if skipped else "")
    )
    return raw


def main() -> None:
    src_path = first_existing(SRC_CANDIDATES)
    if src_path is None:
        raise SystemExit(f"missing source; tried: {SRC_CANDIDATES}")

    print("Loading", src_path)
    g = json.loads(src_path.read_text(encoding="utf-8"))
    raw = read_blob(g)
    raw = ensure_idle_animation(g, raw)
    write_blob(g, bytes(raw))

    # Keep an unscaled source (with Idle) under tools/art for the next rebake.
    ART_DIR.mkdir(parents=True, exist_ok=True)
    art_gltf = ART_DIR / "Player_V2_rigged.gltf"
    art_gltf.write_text(json.dumps(g, separators=(",", ":")), encoding="utf-8")
    print("Cached unscaled source", art_gltf)
    for bb in (
        Path(r"c:\Users\johnr\Documents\Player_V2_rigged.bbmodel"),
        Path(r"c:\Users\johnr\Documents\Models\Player_V2_rigged.bbmodel"),
    ):
        if bb.exists():
            shutil.copy2(bb, ART_DIR / "Player_V2_rigged.bbmodel")
            break
    raw = read_blob(g)

    src_png = first_existing(SRC_PNG_CANDIDATES)
    if src_png is not None:
        tex = Image.open(src_png).convert("RGBA")
        print("Atlas from", src_png)
        shutil.copy2(src_png, ART_DIR / "Player_V2.png")
    else:
        img_uri = g["images"][0]["uri"]
        assert img_uri.startswith("data:image/png;base64,")
        tex = Image.open(io.BytesIO(base64.b64decode(img_uri.split(",", 1)[1]))).convert("RGBA")
        print("Atlas from embedded glTF image")

    print("Cleaning atlas...")
    tex = clean_atlas(tex, pad_radius=12)
    ART_DIR.mkdir(parents=True, exist_ok=True)
    PNG_DST.parent.mkdir(parents=True, exist_ok=True)
    tex.save(PNG_DST, format="PNG")
    shutil.copy2(PNG_DST, ART_DIR / "player.png")
    g["images"] = [{"uri": "player.png"}]
    g["textures"] = [{"source": 0, "sampler": 0}]
    if "samplers" not in g:
        g["samplers"] = [{}]

    # Opaque — avoid MASK punching holes on padded edges
    if g.get("materials"):
        mat = g["materials"][0]
        mat["alphaMode"] = "OPAQUE"
        mat.pop("alphaCutoff", None)
        mat.setdefault("pbrMetallicRoughness", {})["baseColorTexture"] = {"index": 0}
        mat["pbrMetallicRoughness"]["metallicFactor"] = 0
        mat["pbrMetallicRoughness"]["roughnessFactor"] = 1
        # Prefer atlas over solid factor once textured
        mat["pbrMetallicRoughness"].pop("baseColorFactor", None)

    prim = g["meshes"][0]["primitives"][0]
    pos_acc = prim["attributes"]["POSITION"]
    pos = read_accessor(g, raw, pos_acc)
    xs = [p[0] for p in pos]
    ys = [p[1] for p in pos]
    zs = [p[2] for p in pos]
    miny, maxy = min(ys), max(ys)
    height = maxy - miny
    s = TARGET_HEIGHT / height if height > 1e-6 else 1.0
    cx = 0.5 * (min(xs) + max(xs))
    cz = 0.5 * (min(zs) + max(zs))
    tx, ty, tz = -s * cx, -s * miny, -s * cz
    print(f"raw h={height:.4f} -> scale={s:.6f} t=({tx:.4f},{ty:.4f},{tz:.4f}) target={TARGET_HEIGHT}")

    write_f32_accessor(
        g, raw, pos_acc, [(s * x + tx, s * y + ty, s * z + tz) for x, y, z in pos]
    )

    skeleton_root = g["skins"][0].get("skeleton")
    for i, node in enumerate(g["nodes"]):
        if "translation" in node:
            x, y, z = node["translation"]
            node["translation"] = [s * x, s * y, s * z]
        if skeleton_root is not None and i == skeleton_root:
            t = node.get("translation", [0.0, 0.0, 0.0])
            node["translation"] = [t[0] + tx, t[1] + ty, t[2] + tz]

    for node in g["nodes"]:
        if "mesh" in node and "skin" in node:
            node.pop("translation", None)
            node.pop("rotation", None)
            node.pop("scale", None)

    # Joint locals were scaled as pure translations (no joint scale). Recompute
    # IBMs as inv(worldJoint) so bind-pose skinning stays identity. The old
    # IBM * inv(M) path left a 1/s scale in IBM while joints stayed unscaled,
    # which exploded ear/hand verts into floating dots above the head.
    worlds = joint_world_matrices(g)
    for skin in g.get("skins", []):
        ibm_acc = skin.get("inverseBindMatrices")
        if ibm_acc is None:
            continue
        ibms = [mat4_invert(worlds[joint]) for joint in skin["joints"]]
        write_f32_accessor(g, raw, ibm_acc, ibms)

    for anim in g.get("animations", []):
        for ch in anim.get("channels", []):
            if ch.get("target", {}).get("path") != "translation":
                continue
            out_acc = anim["samplers"][ch["sampler"]]["output"]
            vals = read_accessor(g, raw, out_acc)
            write_f32_accessor(g, raw, out_acc, [(s * x, s * y, s * z) for x, y, z in vals])

    raw = ensure_color0(g, raw, prim, tex)
    write_blob(g, bytes(raw))

    g.setdefault("asset", {})["generator"] = "AI RPG Engine bake_player_v2_gltf.py"
    g["asset"]["version"] = "2.0"

    raw2 = read_blob(g)
    pos2 = read_accessor(g, raw2, pos_acc)
    print(
        f"baked mesh y[{min(p[1] for p in pos2):.3f},{max(p[1] for p in pos2):.3f}] "
        f"h={max(p[1] for p in pos2) - min(p[1] for p in pos2):.3f}"
    )

    DST.write_text(json.dumps(g, separators=(",", ":")), encoding="utf-8")
    print("Wrote", DST)
    print("Wrote", PNG_DST)


if __name__ == "__main__":
    main()
