"""
Rig Player_V2 for Blockbench 5 using one continuous body mesh + armature weights.

Keeps the welded body intact (no face splits) so joints have no seam gaps.
Vertex weights drive deformation instead of rigid mesh pieces.
"""
from __future__ import annotations

import json
import math
import uuid
from pathlib import Path

SRC = Path(r"c:\Users\johnr\Documents\Player_V2.bbmodel")
DST = Path(r"c:\Users\johnr\Documents\3DGameIdea\tools\art\player\Player_V2.bbmodel")
DST_DOCS = Path(r"c:\Users\johnr\Documents\Player_V2_rigged.bbmodel")


def new_uuid() -> str:
    return str(uuid.uuid4())


def short_id(used: set[str]) -> str:
    while True:
        s = uuid.uuid4().hex[:4]
        if s not in used:
            used.add(s)
            return s


def rot_matrix(rx, ry, rz):
    ax, ay, az = map(math.radians, (rx, ry, rz))
    cx, sx = math.cos(ax), math.sin(ax)
    cy, sy = math.cos(ay), math.sin(ay)
    cz, sz = math.cos(az), math.sin(az)
    return [
        [cy * cz, cz * sx * sy - cx * sz, sx * sz + cx * cz * sy],
        [cy * sz, cx * cz + sx * sy * sz, cx * sy * sz - cz * sx],
        [-sy, cy * sx, cx * cy],
    ]


def mul(m, v):
    return [
        m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2],
        m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2],
        m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2],
    ]


def world_point(origin, rotation, local):
    r = mul(rot_matrix(*(rotation or [0, 0, 0])), local)
    return [r[0] + origin[0], r[1] + origin[1], r[2] + origin[2]]


def weight_key(mesh_uuid: str, vkey: str) -> str:
    return f"{mesh_uuid[:6]}:{vkey}"


def outliner_node(uid: str, children: list | None = None) -> dict:
    return {"uuid": uid, "isOpen": True, "children": children or []}


def cube_to_mesh(cube: dict, name: str) -> dict:
    """Convert a Blockbench cube to a mesh with verts in LOCAL (origin-relative) space.

    Cube `from`/`to` are world-space corners. Mesh vertices must be local so
    `origin + R * local` recovers the unrigged placement. Storing absolute
    from/to as 'local' while keeping the cube origin doubles the offset and
    floats nose/brows above the head.
    """
    fr, to = cube["from"], cube["to"]
    origin = [float(c) for c in (cube.get("origin") or [0, 0, 0])]
    x0, y0, z0 = fr
    x1, y1, z1 = to
    used: set[str] = set()
    corners_world = {
        "a": [x0, y0, z0],
        "b": [x1, y0, z0],
        "c": [x1, y1, z0],
        "d": [x0, y1, z0],
        "e": [x0, y0, z1],
        "f": [x1, y0, z1],
        "g": [x1, y1, z1],
        "h": [x0, y1, z1],
    }
    vids = {k: short_id(used) for k in corners_world}
    vertices = {
        vids[k]: [corners_world[k][i] - origin[i] for i in range(3)] for k in corners_world
    }
    faces_def = [
        ("a", "b", "c", "d"),
        ("e", "h", "g", "f"),
        ("a", "e", "f", "b"),
        ("d", "c", "g", "h"),
        ("a", "d", "h", "e"),
        ("b", "f", "g", "c"),
    ]
    tex = 0
    for face in (cube.get("faces") or {}).values():
        if isinstance(face, dict) and face.get("texture") is not None:
            tex = face["texture"]
            break
    faces = {}
    for quad in faces_def:
        ids = [vids[q] for q in quad]
        faces[short_id(used)] = {
            "vertices": ids,
            "uv": {ids[0]: [0, 0], ids[1]: [1, 0], ids[2]: [1, 1], ids[3]: [0, 1]},
            "texture": tex,
        }
    return {
        "name": name,
        "uuid": new_uuid(),
        "type": "mesh",
        "color": cube.get("color", 0),
        "origin": origin,
        "rotation": [float(c) for c in (cube.get("rotation") or [0, 0, 0])],
        "shading": "flat",
        "export": True,
        "visibility": True,
        "locked": False,
        "render_order": "default",
        "scope": 0,
        "allow_mirror_modeling": True,
        "vertices": vertices,
        "faces": faces,
    }


def make_bone(name: str, local_origin, length: float, color: int = 0) -> dict:
    return {
        "type": "armature_bone",
        "name": name,
        "uuid": new_uuid(),
        "export": True,
        "locked": False,
        "visibility": True,
        "origin": [float(local_origin[0]), float(local_origin[1]), float(local_origin[2])],
        "rotation": [0, 0, 0],
        "length": float(length),
        "width": 1.5,
        "connected": True,
        "color": color,
        "vertex_weights": {},
        "isOpen": True,
        "children": [],
    }


def add_weight(bone: dict, mesh_uuid: str, vkey: str, w: float) -> None:
    if w <= 0:
        return
    key = weight_key(mesh_uuid, vkey)
    bone["vertex_weights"][key] = bone["vertex_weights"].get(key, 0.0) + w


def smoothstep(edge0: float, edge1: float, x: float) -> float:
    if edge0 == edge1:
        return 0.0 if x < edge0 else 1.0
    t = max(0.0, min(1.0, (x - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)


def classify_body_weights(wx: float, wy: float, wz: float) -> dict[str, float]:
    """
    Return bone_name -> weight for a body-mesh vertex in world space.
    Soft blends near joints so the continuous mesh doesn't tear open.
    """
    ax = abs(wx)
    blend = 1.2  # joint blend half-width

    # --- Arms (T-pose) ---
    if ax > 5.0 and wy >= 24.0:
        side = "Left" if wx >= 0 else "Right"
        # shoulder blend: Chest <-> UpperArm around |x|=6
        shoulder = 6.0
        elbow = 14.5
        wrist = 20.0

        if ax < shoulder + blend:
            # near torso: mix Chest and UpperArm
            t = smoothstep(shoulder - blend, shoulder + blend, ax)
            return {"Chest": 1.0 - t, f"{side}UpperArm": t}

        if ax < elbow + blend:
            t = smoothstep(elbow - blend, elbow + blend, ax)
            if ax < elbow - blend:
                return {f"{side}UpperArm": 1.0}
            return {f"{side}UpperArm": 1.0 - t, f"{side}LowerArm": t}

        t = smoothstep(wrist - blend, wrist + blend, ax)
        if ax < wrist - blend:
            return {f"{side}LowerArm": 1.0}
        return {f"{side}LowerArm": 1.0 - t, f"{side}Hand": t}

    # --- Legs ---
    if ax < 6.5 and wy < 16.5:
        side = "Left" if wx >= 0 else "Right"
        hip = 15.5
        knee = 9.0
        ankle = 3.5

        # hip blend with Hips bone
        if wy > hip - blend:
            t = smoothstep(hip - blend, hip + 0.5, wy)
            # high y near hips mesh → Hips, lower → UpperLeg
            return {"Hips": t, f"{side}UpperLeg": 1.0 - t}

        if wy > knee - blend:
            t = smoothstep(knee - blend, knee + blend, wy)
            if wy > knee + blend:
                return {f"{side}UpperLeg": 1.0}
            return {f"{side}UpperLeg": t, f"{side}LowerLeg": 1.0 - t}

        t = smoothstep(ankle - blend, ankle + blend, wy)
        if wy > ankle + blend:
            return {f"{side}LowerLeg": 1.0}
        return {f"{side}LowerLeg": t, f"{side}Foot": 1.0 - t}

    # --- Torso stack ---
    hips_y, spine_y, chest_y = 16.0, 20.0, 26.0
    if wy < hips_y + blend:
        t = smoothstep(hips_y - blend, hips_y + blend, wy)
        if wy < hips_y - blend:
            return {"Hips": 1.0}
        return {"Hips": 1.0 - t, "Spine": t}
    if wy < spine_y + blend:
        t = smoothstep(spine_y - blend, spine_y + blend, wy)
        if wy < spine_y - blend:
            return {"Spine": 1.0}
        return {"Spine": 1.0 - t, "Chest": t}
    if wy < chest_y + blend:
        t = smoothstep(chest_y - blend, chest_y + blend, wy)
        if wy < chest_y - blend:
            return {"Chest": 1.0}
        # upper chest toward neck
        return {"Chest": 1.0 - t * 0.35, "Neck": t * 0.35} if t > 0 else {"Chest": 1.0}
    # top of torso / neck join
    return {"Chest": 0.25, "Neck": 0.75}


def assign_full_mesh(bone: dict, mesh: dict) -> None:
    for vkey in mesh["vertices"]:
        add_weight(bone, mesh["uuid"], vkey, 1.0)


def main() -> None:
    print("Loading", SRC)
    data = json.loads(SRC.read_text(encoding="utf-8"))
    els = data["elements"]
    assert len(els) == 18

    head, neck, brow_r, brow_l, body, nose = (
        els[0],
        els[1],
        els[2],
        els[3],
        els[4],
        els[5],
    )
    head["name"] = "HeadMesh"
    neck["name"] = "NeckMesh"
    body["name"] = "BodyMesh"
    for el in (head, neck, body):
        el["locked"] = False
        el["visibility"] = True

    right_idxs = [6, 7, 9, 11, 13, 15]
    left_idxs = [8, 10, 12, 14, 16, 17]
    finger_meshes = []
    for i, idx in enumerate(left_idxs, 1):
        els[idx]["name"] = f"LeftFinger{i}"
        els[idx]["locked"] = False
        finger_meshes.append(els[idx])
    for i, idx in enumerate(right_idxs, 1):
        els[idx]["name"] = f"RightFinger{i}"
        els[idx]["locked"] = False
        finger_meshes.append(els[idx])

    brow_r_m = cube_to_mesh(brow_r, "BrowRight")
    brow_l_m = cube_to_mesh(brow_l, "BrowLeft")
    nose_m = cube_to_mesh(nose, "Nose")

    # Keep original continuous body — do NOT split
    all_meshes = [head, neck, body, brow_r_m, brow_l_m, nose_m] + finger_meshes

    W = {
        "Hips": [0.0, 16.0, 0.5],
        "Spine": [0.0, 20.0, 0.5],
        "Chest": [0.0, 27.0, 0.5],
        "Neck": [0.0, 32.3, 0.6],
        "Head": [0.0, 33.0, 0.0],
        "LeftUpperArm": [5.0, 29.5, 0.5],
        "LeftLowerArm": [14.5, 29.5, 1.0],
        "LeftHand": [21.0, 29.5, 1.2],
        "RightUpperArm": [-5.0, 29.5, 0.5],
        "RightLowerArm": [-14.5, 29.5, 1.0],
        "RightHand": [-21.0, 29.5, 1.2],
        "LeftUpperLeg": [2.2, 15.5, 0.5],
        "LeftLowerLeg": [2.2, 9.0, 0.5],
        "LeftFoot": [2.2, 2.5, 0.5],
        "RightUpperLeg": [-2.2, 15.5, 0.5],
        "RightLowerLeg": [-2.2, 9.0, 0.5],
        "RightFoot": [-2.2, 2.5, 0.5],
    }

    def local(child: str, parent: str | None) -> list[float]:
        if parent is None:
            return list(W[child])
        return [W[child][i] - W[parent][i] for i in range(3)]

    def bone_len(name: str, child: str | None) -> float:
        if not child:
            return 4.0
        d = [W[child][i] - W[name][i] for i in range(3)]
        return max(2.0, math.sqrt(sum(x * x for x in d)))

    bones = {
        "Hips": make_bone("Hips", local("Hips", None), bone_len("Hips", "Spine"), 0),
        "Spine": make_bone("Spine", local("Spine", "Hips"), bone_len("Spine", "Chest"), 0),
        "Chest": make_bone("Chest", local("Chest", "Spine"), bone_len("Chest", "Neck"), 0),
        "Neck": make_bone("Neck", local("Neck", "Chest"), bone_len("Neck", "Head"), 5),
        "Head": make_bone("Head", local("Head", "Neck"), 4.0, 5),
        "LeftUpperArm": make_bone(
            "LeftUpperArm", local("LeftUpperArm", "Chest"), bone_len("LeftUpperArm", "LeftLowerArm"), 1
        ),
        "LeftLowerArm": make_bone(
            "LeftLowerArm",
            local("LeftLowerArm", "LeftUpperArm"),
            bone_len("LeftLowerArm", "LeftHand"),
            1,
        ),
        "LeftHand": make_bone("LeftHand", local("LeftHand", "LeftLowerArm"), 3.0, 1),
        "RightUpperArm": make_bone(
            "RightUpperArm",
            local("RightUpperArm", "Chest"),
            bone_len("RightUpperArm", "RightLowerArm"),
            2,
        ),
        "RightLowerArm": make_bone(
            "RightLowerArm",
            local("RightLowerArm", "RightUpperArm"),
            bone_len("RightLowerArm", "RightHand"),
            2,
        ),
        "RightHand": make_bone("RightHand", local("RightHand", "RightLowerArm"), 3.0, 2),
        "LeftUpperLeg": make_bone(
            "LeftUpperLeg", local("LeftUpperLeg", "Hips"), bone_len("LeftUpperLeg", "LeftLowerLeg"), 3
        ),
        "LeftLowerLeg": make_bone(
            "LeftLowerLeg",
            local("LeftLowerLeg", "LeftUpperLeg"),
            bone_len("LeftLowerLeg", "LeftFoot"),
            3,
        ),
        "LeftFoot": make_bone("LeftFoot", local("LeftFoot", "LeftLowerLeg"), 3.0, 3),
        "RightUpperLeg": make_bone(
            "RightUpperLeg",
            local("RightUpperLeg", "Hips"),
            bone_len("RightUpperLeg", "RightLowerLeg"),
            4,
        ),
        "RightLowerLeg": make_bone(
            "RightLowerLeg",
            local("RightLowerLeg", "RightUpperLeg"),
            bone_len("RightLowerLeg", "RightFoot"),
            4,
        ),
        "RightFoot": make_bone("RightFoot", local("RightFoot", "RightLowerLeg"), 3.0, 4),
    }

    # Rigid accessory meshes
    assign_full_mesh(bones["Head"], head)
    assign_full_mesh(bones["Head"], brow_l_m)
    assign_full_mesh(bones["Head"], brow_r_m)
    assign_full_mesh(bones["Head"], nose_m)
    assign_full_mesh(bones["Neck"], neck)
    for i, idx in enumerate(left_idxs, 1):
        assign_full_mesh(bones["LeftHand"], els[idx])
    for i, idx in enumerate(right_idxs, 1):
        assign_full_mesh(bones["RightHand"], els[idx])

    # Continuous body: soft weights, no topology cuts
    print("Painting body vertex weights...")
    body_origin = body.get("origin") or [0, 0, 0]
    body_rot = body.get("rotation") or [0, 0, 0]
    weight_counts = {k: 0 for k in bones}
    for vkey, local_v in body["vertices"].items():
        wx, wy, wz = world_point(body_origin, body_rot, local_v)
        weights = classify_body_weights(wx, wy, wz)
        total = sum(weights.values()) or 1.0
        for bone_name, w in weights.items():
            nw = w / total
            if nw > 0.001:
                add_weight(bones[bone_name], body["uuid"], vkey, nw)
                weight_counts[bone_name] += 1
    print("  body verts influenced:", {k: v for k, v in weight_counts.items() if v})

    def link(parent: str, *kids: str) -> None:
        bones[parent]["children"] = [bones[k]["uuid"] for k in kids]

    link("Hips", "Spine", "LeftUpperLeg", "RightUpperLeg")
    link("Spine", "Chest")
    link("Chest", "Neck", "LeftUpperArm", "RightUpperArm")
    link("Neck", "Head")
    link("LeftUpperArm", "LeftLowerArm")
    link("LeftLowerArm", "LeftHand")
    link("RightUpperArm", "RightLowerArm")
    link("RightLowerArm", "RightHand")
    link("LeftUpperLeg", "LeftLowerLeg")
    link("LeftLowerLeg", "LeftFoot")
    link("RightUpperLeg", "RightLowerLeg")
    link("RightLowerLeg", "RightFoot")

    bone_list = list(bones.values())
    mesh_uuids = [m["uuid"] for m in all_meshes]
    armature = {
        "type": "armature",
        "name": "PlayerArmature",
        "uuid": new_uuid(),
        "export": True,
        "locked": False,
        "visibility": True,
        "isOpen": True,
        "children": [bones["Hips"]["uuid"]] + mesh_uuids,
    }

    def bone_tree(name: str) -> dict:
        kids = []
        for child_uuid in bones[name]["children"]:
            child_name = next(b["name"] for b in bone_list if b["uuid"] == child_uuid)
            kids.append(bone_tree(child_name))
        return outliner_node(bones[name]["uuid"], kids)

    data["elements"] = all_meshes + [armature] + bone_list
    data["groups"] = []
    data["outliner"] = [
        outliner_node(armature["uuid"], mesh_uuids + [bone_tree("Hips")])
    ]
    data["name"] = "Player_V2"

    payload = json.dumps(data, separators=(",", ":"))
    DST.parent.mkdir(parents=True, exist_ok=True)
    DST.write_text(payload, encoding="utf-8")
    DST_DOCS.write_text(payload, encoding="utf-8")
    print("Wrote", DST)
    print("Wrote", DST_DOCS)
    print(f"meshes={len(all_meshes)} (body kept whole) bones={len(bone_list)}")


if __name__ == "__main__":
    main()
