"""Build Player_V3 from Ashfell Blade turnaround (bald, T-pose, palms down).

Reference: context/art/reference/starting-player-ashfell-blade-turnaround.png
- No hair
- Eyes/mouth painted later (not modeled)
- Brows + nose + ears modeled
- Match front + side orthos; denser loft loops for vertex control
"""
from __future__ import annotations

import json
import math
import uuid
from pathlib import Path

OUT = Path(__file__).resolve().parent / "Player_V3.bbmodel"

# Scale: feet y=0, head top ~40.5 (matches existing player BB scale)


def uid() -> str:
    return str(uuid.uuid4())


def mesh(name, origin, verts_local, faces_idx, color=0) -> dict:
    verts = {f"v{i}": list(v) for i, v in enumerate(verts_local)}
    faces = {str(fi): {"vertices": [f"v{i}" for i in idxs]} for fi, idxs in enumerate(faces_idx)}
    return {
        "name": name,
        "uuid": uid(),
        "type": "mesh",
        "origin": list(origin),
        "rotation": [0, 0, 0],
        "color": color,
        "export": True,
        "visibility": True,
        "locked": False,
        "shading": "flat",
        "vertices": verts,
        "faces": faces,
    }


def box_mesh(name, cx, cy, cz, sx, sy, sz, origin, color=0) -> dict:
    hx, hy, hz = sx / 2, sy / 2, sz / 2
    ox, oy, oz = origin
    v = [
        [cx - hx - ox, cy - hy - oy, cz - hz - oz],
        [cx + hx - ox, cy - hy - oy, cz - hz - oz],
        [cx + hx - ox, cy + hy - oy, cz - hz - oz],
        [cx - hx - ox, cy + hy - oy, cz - hz - oz],
        [cx - hx - ox, cy - hy - oy, cz + hz - oz],
        [cx + hx - ox, cy - hy - oy, cz + hz - oz],
        [cx + hx - ox, cy + hy - oy, cz + hz - oz],
        [cx - hx - ox, cy + hy - oy, cz + hz - oz],
    ]
    f = [
        [0, 1, 2, 3],
        [4, 7, 6, 5],
        [0, 4, 5, 1],
        [3, 2, 6, 7],
        [0, 3, 7, 4],
        [1, 5, 6, 2],
    ]
    return mesh(name, origin, v, f, color)


def ellipse_x(name, sides, ry0, rz0, ry1, rz1, x0, x1, y, z, origin, color=0) -> dict:
    ox, oy, oz = origin
    ring0, ring1 = [], []
    for i in range(sides):
        a = (i / sides) * math.pi * 2
        c, s = math.cos(a), math.sin(a)
        ring0.append([x0 - ox, y + c * ry0 - oy, z + s * rz0 - oz])
        ring1.append([x1 - ox, y + c * ry1 - oy, z + s * rz1 - oz])
    verts = ring0 + ring1
    faces = []
    for i in range(sides):
        j = (i + 1) % sides
        faces.append([i, j, sides + j, sides + i])
    faces.append(list(reversed(range(sides))))
    faces.append(list(range(sides, 2 * sides)))
    return mesh(name, origin, verts, faces, color)


def ellipse_y(name, sides, rx0, rz0, rx1, rz1, x, y0, y1, z, origin, color=0) -> dict:
    ox, oy, oz = origin
    ring0, ring1 = [], []
    for i in range(sides):
        a = (i / sides) * math.pi * 2
        c, s = math.cos(a), math.sin(a)
        ring0.append([x + c * rx0 - ox, y0 - oy, z + s * rz0 - oz])
        ring1.append([x + c * rx1 - ox, y1 - oy, z + s * rz1 - oz])
    verts = ring0 + ring1
    faces = []
    for i in range(sides):
        j = (i + 1) % sides
        faces.append([i, j, sides + j, sides + i])
    faces.append(list(reversed(range(sides))))
    faces.append(list(range(sides, 2 * sides)))
    return mesh(name, origin, verts, faces, color)


def loft(name, rings, origin, color=0) -> dict:
    ox, oy, oz = origin
    verts, key_rings = [], []
    for ring in rings:
        keys = []
        for p in ring:
            keys.append(len(verts))
            verts.append([p[0] - ox, p[1] - oy, p[2] - oz])
        key_rings.append(keys)
    faces = []
    n = len(key_rings[0])
    for ri in range(len(key_rings) - 1):
        a, b = key_rings[ri], key_rings[ri + 1]
        for i in range(n):
            j = (i + 1) % n
            faces.append([a[i], a[j], b[j], b[i]])
    faces.append(list(reversed(key_rings[0])))
    faces.append(list(key_rings[-1]))
    return mesh(name, origin, verts, faces, color)


def head_ring(y, w, front, back):
    """Boxier head cross-section (Ashfell side: flat face + flat back, not egg)."""
    mid = (front + back) * 0.08
    return [
        [w * 0.85, y, front],
        [w, y, front * 0.15 + mid],
        [w, y, back * 0.2 + mid],
        [w * 0.85, y, back],
        [0.0, y, back],
        [-w * 0.85, y, back],
        [-w, y, back * 0.2 + mid],
        [-w, y, front * 0.15 + mid],
        [-w * 0.85, y, front],
        [0.0, y, front],
    ]


def torso_ring(y, w, front, back):
    """Flat-chested tunic ring for side silhouette."""
    return [
        [w * 0.88, y, front],
        [w, y, (front + back) * 0.12],
        [w * 0.88, y, back],
        [0.0, y, back],
        [-w * 0.88, y, back],
        [-w, y, (front + back) * 0.12],
        [-w * 0.88, y, front],
        [0.0, y, front],
    ]


def group(name, origin, children, color=0) -> dict:
    return {
        "name": name,
        "uuid": uid(),
        "type": "group",
        "origin": list(origin),
        "rotation": [0, 0, 0],
        "color": color,
        "export": True,
        "visibility": True,
        "locked": False,
        "children": children,
    }


def build():
    els = []

    # --- HEAD (bald, boxy jaw, dense loops) ---
    # Concept side: flat face plane, protruding brow/nose, thick neck seat
    els.append(
        loft(
            "Head",
            [
                # denser loops; flatter crown; proud face plane for paint + brows
                head_ring(32.5, 2.65, -2.5, 2.2),
                head_ring(33.1, 3.15, -2.95, 2.6),
                head_ring(33.8, 3.35, -3.25, 2.85),
                head_ring(34.55, 3.45, -3.4, 2.95),
                head_ring(35.3, 3.5, -3.5, 3.05),
                head_ring(36.05, 3.5, -3.55, 3.1),
                head_ring(36.8, 3.45, -3.55, 3.1),
                head_ring(37.55, 3.35, -3.45, 3.0),
                head_ring(38.3, 3.15, -3.2, 2.85),
                head_ring(39.05, 2.75, -2.75, 2.5),
                head_ring(39.7, 2.2, -2.2, 2.1),
                head_ring(40.2, 1.55, -1.55, 1.5),
                head_ring(40.55, 0.95, -0.95, 0.95),
            ],
            [0, 36.3, 0],
            1,
        )
    )
    oh = [0, 36.3, 0]
    els += [
        box_mesh("EarL", 3.6, 36.55, 0.25, 0.55, 1.6, 1.2, oh, 1),
        box_mesh("EarR", -3.6, 36.55, 0.25, 0.55, 1.6, 1.2, oh, 1),
        # Thick modeled brows (concept side shows them proud of forehead)
        box_mesh("BrowL", 1.55, 37.65, -3.7, 1.75, 0.55, 0.7, oh, 5),
        box_mesh("BrowR", -1.55, 37.65, -3.7, 1.75, 0.55, 0.7, oh, 5),
        box_mesh("Nose", 0, 36.15, -4.2, 0.75, 0.85, 1.25, oh, 1),
    ]

    els.append(
        ellipse_y("Neck", 6, 1.65, 1.5, 1.85, 1.65, 0, 31.0, 32.7, 0.2, [0, 31.8, 0], 1)
    )

    # --- TUNIC (hem hangs past belt/hips; denser loops; flat side depth) ---
    els.append(
        loft(
            "Tunic",
            [
                torso_ring(16.2, 4.55, -2.85, 3.15),  # flared hem
                torso_ring(17.2, 4.5, -2.75, 3.05),
                torso_ring(18.2, 4.55, -2.75, 3.05),
                torso_ring(19.4, 4.65, -2.8, 3.1),
                torso_ring(20.8, 4.8, -2.85, 3.15),
                torso_ring(22.4, 5.0, -2.95, 3.25),
                torso_ring(24.0, 5.2, -3.0, 3.3),
                torso_ring(25.6, 5.4, -3.05, 3.35),
                torso_ring(27.2, 5.6, -3.1, 3.35),
                torso_ring(28.6, 5.75, -3.15, 3.3),
                torso_ring(29.8, 5.65, -3.1, 3.2),
                torso_ring(31.0, 5.25, -2.95, 2.95),
            ],
            [0, 24, 0],
            2,
        )
    )
    els += [
        box_mesh("VNeck", 0, 30.35, -2.95, 2.15, 2.35, 0.6, [0, 30.3, 0], 5),
        box_mesh("NeckCordA", -0.35, 29.7, -3.15, 0.55, 1.0, 0.45, [0, 29.7, 0], 5),
        box_mesh("NeckCordB", 0.35, 29.7, -3.15, 0.55, 1.0, 0.45, [0, 29.7, 0], 5),
    ]

    # Short sleeves (mid-bicep) — tunic color
    els += [
        ellipse_x("SleeveL", 8, 2.05, 2.25, 1.9, 2.1, 4.9, 9.2, 29.1, 0.12, [5.6, 29.1, 0], 2),
        ellipse_x("SleeveR", 8, 2.05, 2.25, 1.9, 2.1, -4.9, -9.2, 29.1, 0.12, [-5.6, 29.1, 0], 2),
        # Shoulder caps so armpit reads connected
        box_mesh("ShoulderL", 4.6, 29.3, 0.1, 2.4, 3.4, 3.5, [5.6, 29.1, 0], 2),
        box_mesh("ShoulderR", -4.6, 29.3, 0.1, 2.4, 3.4, 3.5, [-5.6, 29.1, 0], 2),
    ]

    def arm(sign: int, pref: str):
        o = [sign * 5.6, 29.1, 0]
        # Exposed upper arm (skin) past sleeve
        parts = [
            ellipse_x(
                f"{pref}UpperArm", 8, 1.55, 1.85, 1.4, 1.65,
                sign * 9.0, sign * 14.0, 29.1, 0.15, o, 1
            ),
            # Forearm wraps (3 bands) elbow→wrist
            ellipse_x(
                f"{pref}Wrap1", 8, 1.6, 1.9, 1.55, 1.8,
                sign * 13.8, sign * 15.7, 29.1, 0.18, o, 5
            ),
            ellipse_x(
                f"{pref}Wrap2", 8, 1.55, 1.8, 1.45, 1.7,
                sign * 15.7, sign * 17.6, 29.1, 0.2, o, 5
            ),
            ellipse_x(
                f"{pref}Wrap3", 8, 1.4, 1.6, 1.25, 1.4,
                sign * 17.6, sign * 19.7, 29.1, 0.22, o, 5
            ),
        ]
        # Palms DOWN: thin Y, wide Z — but chunky enough to read fingers from 3/4
        pcx = sign * 21.0
        parts.append(box_mesh(f"{pref}Palm", pcx, 29.1, 0.0, 2.9, 1.15, 3.5, o, 1))
        for fname, z, length, tw in (
            ("Index", 1.2, 2.7, 0.65),
            ("Middle", 0.35, 2.9, 0.68),
            ("Ring", -0.45, 2.7, 0.65),
            ("Pinky", -1.2, 2.3, 0.55),
        ):
            cx = sign * (22.2 + length / 2)
            parts.append(box_mesh(f"{pref}{fname}", cx, 29.1, z, length, 0.9, tw, o, 1))
        parts.append(box_mesh(f"{pref}Thumb", sign * 20.2, 29.1, 1.95, 1.7, 0.8, 0.9, o, 1))
        return parts

    els.extend(arm(1, "L"))
    els.extend(arm(-1, "R"))

    # Belt sits on hips under tunic hem visually (slightly proud)
    els += [
        box_mesh("Belt", 0, 17.9, 0.15, 9.5, 1.5, 6.2, [0, 17.9, 0], 5),
        box_mesh("BeltKnot", 0.7, 16.9, -3.35, 1.9, 2.3, 1.25, [0.7, 16.9, -3.2], 5),
        box_mesh("BeltTail", 0.7, 14.8, -3.2, 0.9, 2.2, 0.9, [0.7, 15, -3.1], 5),
        # Pouch — character LEFT hip (+X)
        box_mesh("Pouch", 4.4, 15.5, 0.25, 2.3, 3.0, 2.5, [4.4, 15.5, 0], 5),
        box_mesh("PouchFlap", 4.4, 16.85, 0.25, 2.4, 0.55, 2.6, [4.4, 16.85, 0], 5),
    ]

    for sign, pref in ((1, "L"), (-1, "R")):
        x = sign * 2.4
        ol = [x, 10, 0]
        els += [
            ellipse_y(f"{pref}Thigh", 8, 2.25, 2.05, 2.0, 1.85, x, 17.5, 9.6, 0.1, ol, 5),
            # Knee loop (secondary form)
            ellipse_y(f"{pref}Knee", 8, 2.05, 1.85, 1.95, 1.75, x, 9.6, 8.5, 0.1, ol, 5),
            ellipse_y(f"{pref}Shin", 8, 1.8, 1.6, 1.6, 1.4, x, 8.5, 4.2, 0.08, ol, 5),
            ellipse_y(f"{pref}BootShaft", 8, 1.95, 1.75, 1.75, 1.55, x, 4.4, 1.35, 0.05, [x, 2.9, 0], 5),
            box_mesh(f"{pref}BootCuff", x, 4.15, 0.05, 4.3, 0.95, 3.9, [x, 4.15, 0], 5),
            box_mesh(f"{pref}Foot", x, 0.7, -1.05, 3.05, 1.4, 4.8, [x, 0.7, -0.6], 5),
            box_mesh(f"{pref}Heel", x, 0.55, 1.75, 2.7, 1.15, 1.5, [x, 0.55, 1.55], 5),
        ]

    by = {e["name"]: e["uuid"] for e in els}

    def ids(*ns):
        return [by[n] for n in ns]

    outliner = [
        group(
            "Player_V3",
            [0, 0, 0],
            [
                group(
                    "Hips",
                    [0, 17.9, 0],
                    [
                        group(
                            "Torso",
                            [0, 24, 0],
                            [
                                *ids(
                                    "Tunic",
                                    "VNeck",
                                    "NeckCordA",
                                    "NeckCordB",
                                    "Belt",
                                    "BeltKnot",
                                    "BeltTail",
                                    "Pouch",
                                    "PouchFlap",
                                ),
                                group(
                                    "Chest",
                                    [0, 29, 0],
                                    [
                                        group(
                                            "Neck",
                                            [0, 31.8, 0],
                                            [
                                                by["Neck"],
                                                group(
                                                    "Head",
                                                    [0, 36.3, 0],
                                                    ids("Head", "EarL", "EarR", "BrowL", "BrowR", "Nose"),
                                                ),
                                            ],
                                        ),
                                        group(
                                            "LeftArm",
                                            [5.6, 29.1, 0],
                                            [
                                                *ids("ShoulderL", "SleeveL"),
                                                *ids(
                                                    "LUpperArm",
                                                    "LWrap1",
                                                    "LWrap2",
                                                    "LWrap3",
                                                    "LPalm",
                                                    "LIndex",
                                                    "LMiddle",
                                                    "LRing",
                                                    "LPinky",
                                                    "LThumb",
                                                ),
                                            ],
                                        ),
                                        group(
                                            "RightArm",
                                            [-5.6, 29.1, 0],
                                            [
                                                *ids("ShoulderR", "SleeveR"),
                                                *ids(
                                                    "RUpperArm",
                                                    "RWrap1",
                                                    "RWrap2",
                                                    "RWrap3",
                                                    "RPalm",
                                                    "RIndex",
                                                    "RMiddle",
                                                    "RRing",
                                                    "RPinky",
                                                    "RThumb",
                                                ),
                                            ],
                                        ),
                                    ],
                                ),
                            ],
                        ),
                        group(
                            "LeftLeg",
                            [2.4, 10, 0],
                            ids(
                                "LThigh",
                                "LKnee",
                                "LShin",
                                "LBootShaft",
                                "LBootCuff",
                                "LFoot",
                                "LHeel",
                            ),
                        ),
                        group(
                            "RightLeg",
                            [-2.4, 10, 0],
                            ids(
                                "RThigh",
                                "RKnee",
                                "RShin",
                                "RBootShaft",
                                "RBootCuff",
                                "RFoot",
                                "RHeel",
                            ),
                        ),
                    ],
                )
            ],
        )
    ]

    model = {
        "meta": {"format_version": "5.0", "model_format": "free", "box_uv": False},
        "name": "Player_V3",
        "model_identifier": "",
        "visible_box": [1, 1, 0],
        "variable_placeholders": "",
        "variable_placeholder_buttons": [],
        "resolution": {"width": 16, "height": 16},
        "elements": els,
        "outliner": outliner,
        "textures": [],
        "animations": [],
    }
    OUT.write_text(json.dumps(model, indent="\t"), encoding="utf-8")
    print(f"Wrote {OUT} ({len(els)} meshes)")


if __name__ == "__main__":
    build()
