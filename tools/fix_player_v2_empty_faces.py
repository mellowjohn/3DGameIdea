"""Fix empty (transparent-atlas) faces + inverted winding on Player_V2_rigged.bbmodel."""
from __future__ import annotations

import base64
import io
import json
import math
import shutil
from collections import defaultdict, deque
from pathlib import Path

from PIL import Image

SRC = Path(r"c:\Users\johnr\Documents\Player_V2_rigged.bbmodel")
BACKUP = Path(r"c:\Users\johnr\Documents\Player_V2_rigged.bbmodel.bak")
REPO_COPY = Path(r"c:\Users\johnr\Documents\3DGameIdea\tools\art\player\Player_V2_rigged.bbmodel")


def cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def face_normal(pts):
    n = (0.0, 0.0, 0.0)
    for i in range(1, len(pts) - 1):
        n = add(n, cross(sub(pts[i], pts[0]), sub(pts[i + 1], pts[0])))
    return n


def face_centroid(pts):
    return (
        sum(p[0] for p in pts) / len(pts),
        sum(p[1] for p in pts) / len(pts),
        sum(p[2] for p in pts) / len(pts),
    )


def flip_face(face: dict) -> None:
    face["vertices"] = list(reversed(face["vertices"]))


def remove_duplicate_faces(faces: dict) -> int:
    by_verts: dict[tuple, list[str]] = defaultdict(list)
    for fid, f in faces.items():
        by_verts[tuple(sorted(f["vertices"]))].append(fid)
    removed = 0
    for ids in by_verts.values():
        if len(ids) < 2:
            continue
        for fid in ids[1:]:
            del faces[fid]
            removed += 1
    return removed


def build_edge_adjacency(faces: dict):
    edge_map: dict[tuple, list[tuple[str, tuple]]] = defaultdict(list)
    for fid, f in faces.items():
        vs = f["vertices"]
        for i in range(len(vs)):
            a, b = vs[i], vs[(i + 1) % len(vs)]
            edge_map[tuple(sorted((a, b)))].append((fid, (a, b)))
    return edge_map


def faces_agree_on_edge(dir_a, dir_b) -> bool:
    return dir_a[0] == dir_b[1] and dir_a[1] == dir_b[0]


def mesh_center(verts: dict):
    pts = [tuple(p) for p in verts.values()]
    return (
        sum(p[0] for p in pts) / len(pts),
        sum(p[1] for p in pts) / len(pts),
        sum(p[2] for p in pts) / len(pts),
    )


def make_winding_consistent(faces: dict, verts: dict) -> tuple[int, int]:
    if not faces:
        return 0, 0
    edge_map = build_edge_adjacency(faces)
    neighbors: dict[str, list[tuple[str, tuple, tuple]]] = defaultdict(list)
    # Only manifold edges (exactly two faces) — overshared edges create flip conflicts.
    for entries in edge_map.values():
        if len(entries) != 2:
            continue
        (f0, d0), (f1, d1) = entries
        neighbors[f0].append((f1, d0, d1))
        neighbors[f1].append((f0, d1, d0))

    visited: set[str] = set()
    flipped_total = 0
    components = 0
    center = mesh_center(verts)

    for seed in list(faces.keys()):
        if seed in visited:
            continue
        components += 1
        need_flip: dict[str, bool] = {seed: False}
        queue = deque([seed])
        visited.add(seed)
        while queue:
            cur = queue.popleft()
            cur_flip = need_flip[cur]
            for other, d_cur, d_other in neighbors.get(cur, []):
                if other in need_flip:
                    continue
                d_cur_eff = (d_cur[1], d_cur[0]) if cur_flip else d_cur
                agree = faces_agree_on_edge(d_cur_eff, d_other)
                need_flip[other] = not agree
                visited.add(other)
                queue.append(other)

        component_faces = list(need_flip.keys())

        def score(flip_all: bool) -> float:
            total = 0.0
            for fid in component_faces:
                vs = list(faces[fid]["vertices"])
                if need_flip[fid] ^ flip_all:
                    vs = list(reversed(vs))
                pts = [tuple(verts[v]) for v in vs]
                n = face_normal(pts)
                c = face_centroid(pts)
                to_c = sub(c, center)
                total += n[0] * to_c[0] + n[1] * to_c[1] + n[2] * to_c[2]
            return total

        flip_all = score(True) > score(False)
        for fid in component_faces:
            if need_flip[fid] ^ flip_all:
                flip_face(faces[fid])
                flipped_total += 1
    return flipped_total, components


def build_nearest_opaque(img: Image.Image):
    tw, th = img.size
    pix = img.load()
    nearest = [[None] * tw for _ in range(th)]
    dist = [[10**9] * tw for _ in range(th)]
    q = deque()
    for y in range(th):
        for x in range(tw):
            if pix[x, y][3] < 8:
                continue
            nearest[y][x] = (x, y)
            dist[y][x] = 0
            q.append((x, y))
    while q:
        x, y = q.popleft()
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, ny = x + dx, y + dy
            if 0 <= nx < tw and 0 <= ny < th and dist[ny][nx] > dist[y][x] + 1:
                dist[ny][nx] = dist[y][x] + 1
                nearest[ny][nx] = nearest[y][x]
                q.append((nx, ny))
    return nearest, pix, tw, th


def snap_transparent_uvs(data: dict) -> int:
    tex = data["textures"][0]
    img = Image.open(io.BytesIO(base64.b64decode(tex["source"].split(",", 1)[1]))).convert(
        "RGBA"
    )
    nearest, pix, tw, th = build_nearest_opaque(img)
    rw = data["resolution"]["width"]
    rh = data["resolution"]["height"]
    sx, sy = tw / rw, th / rh
    snapped = 0

    for e in data["elements"]:
        faces = e.get("faces")
        if not isinstance(faces, dict):
            continue
        for f in faces.values():
            uv = f.get("uv")
            if not isinstance(uv, dict):
                continue
            for vid, uvp in list(uv.items()):
                u, v = float(uvp[0]), float(uvp[1])
                x = int(max(0, min(tw - 1, math.floor(u * sx))))
                y = int(max(0, min(th - 1, math.floor(v * sy))))
                if pix[x, y][3] >= 8:
                    continue
                nx, ny = nearest[y][x]
                # Land strictly inside the opaque texel so floor(u*sx) == nx.
                uv[vid] = [(nx + 0.5) / sx, (ny + 0.5) / sy]
                snapped += 1
    return snapped


def verify(data: dict) -> None:
    tex = data["textures"][0]
    img = Image.open(io.BytesIO(base64.b64decode(tex["source"].split(",", 1)[1]))).convert(
        "RGBA"
    )
    pix = img.load()
    tw, th = img.size
    rw, rh = data["resolution"]["width"], data["resolution"]["height"]
    sx, sy = tw / rw, th / rh
    empty = 0
    total = 0
    for e in data["elements"]:
        faces = e.get("faces")
        verts = e.get("vertices")
        if not isinstance(faces, dict) or not isinstance(verts, dict):
            continue
        for f in faces.values():
            total += 1
            uv = f.get("uv") or {}
            ids = f.get("vertices") or []
            coords = [uv[v] for v in ids if v in uv]
            if not coords:
                empty += 1
                continue
            cu = sum(c[0] for c in coords) / len(coords)
            cv = sum(c[1] for c in coords) / len(coords)
            x = int(max(0, min(tw - 1, math.floor(cu * sx))))
            y = int(max(0, min(th - 1, math.floor(cv * sy))))
            if pix[x, y][3] < 8:
                empty += 1
        # count transparent corners too
        # (printed once at end via empty centroids)
        edge_map = build_edge_adjacency(faces)
        conflicts = 0
        manifold = 0
        for entries in edge_map.values():
            if len(entries) != 2:
                continue
            manifold += 1
            (_, d0), (_, d1) = entries
            if not faces_agree_on_edge(d0, d1):
                conflicts += 1
        print(
            f"  {e.get('name')}: faces={len(faces)} winding_conflicts={conflicts}/{manifold}"
        )
    print(f"  transparent-centroid faces: {empty}/{total}")


def main() -> None:
    src_path = BACKUP if BACKUP.exists() else SRC
    data = json.loads(src_path.read_text(encoding="utf-8"))
    if not BACKUP.exists():
        shutil.copy2(SRC, BACKUP)
        print(f"backup -> {BACKUP}")
    else:
        print(f"restoring from {BACKUP}")

    total_removed = total_flipped = 0
    for elem in data["elements"]:
        faces = elem.get("faces")
        verts = elem.get("vertices")
        if not isinstance(faces, dict) or not isinstance(verts, dict):
            continue
        name = elem.get("name", "?")
        removed = remove_duplicate_faces(faces)
        flipped, comps = make_winding_consistent(faces, verts)
        total_removed += removed
        total_flipped += flipped
        print(f"{name}: removed_dups={removed} flipped={flipped} components={comps}")

    snapped = snap_transparent_uvs(data)
    print(f"snapped transparent UV corners: {snapped}")
    print("verify:")
    verify(data)

    text = json.dumps(data, separators=(",", ":"))
    SRC.write_text(text, encoding="utf-8")
    REPO_COPY.write_text(text, encoding="utf-8")
    print(f"wrote {SRC}")
    print(f"wrote {REPO_COPY}")


if __name__ == "__main__":
    main()
