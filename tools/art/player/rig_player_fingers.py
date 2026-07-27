#!/usr/bin/env python3
"""Add finger/thumb bones to Player_V2_rigged.bbmodel and reweight hand verts."""
from __future__ import annotations

import json
import shutil
import uuid
from copy import deepcopy
from pathlib import Path

SRC = Path(r"c:\Users\johnr\Documents\Player_V2_rigged.bbmodel")
REPO_COPY = Path(r"c:\Users\johnr\Documents\3DGameIdea\tools\art\player\Player_V2_rigged.bbmodel")
BACKUP = Path(r"c:\Users\johnr\Documents\Player_V2_rigged.bbmodel.bak_before_fingers")

BODY_MESH = "BodyMesh"
MESH_PREFIX = "5d1157"  # BodyMesh uuid without dashes, first 6


def new_uuid() -> str:
    return str(uuid.uuid4())


def make_bone(
    name: str,
    origin: list[float],
    length: float,
    *,
    color: int = 0,
    connected: bool = False,
    rotation: list[float] | None = None,
    children: list[str] | None = None,
) -> dict:
    return {
        "isOpen": True,
        "uuid": new_uuid(),
        "type": "armature_bone",
        "name": name,
        "children": children or [],
        "export": True,
        "locked": False,
        "scope": 0,
        "origin": origin,
        "rotation": rotation or [0, 0, 0],
        "length": length,
        "width": 0.5,
        "connected": connected,
        "color": color,
        "vertex_weights": {},
        "allow_mirror_modeling": True,
    }


def classify_digit(ax: float, y: float, z: float, w: float) -> tuple[str, str] | None:
    """Classify using left-hand +X space (ax = x for left, -x for right)."""
    if w < 0.5:
        return None  # wrist blend stays on hand

    # Thumb sits on +Z side with lower ax than the index strip
    if z >= 1.15 and ax < 21.48:
        joint = "dist" if (ax >= 21.25 or z >= 1.40) else "prox"
        return ("thumb", joint)

    # Palm / wrist bulk (keep on hand bone)
    if ax < 21.20:
        return None

    # Four fingers by Z (pinky at -Z … index beside thumb)
    if z >= 1.05:
        joint = "dist" if ax >= 22.0 else "prox"
        return ("index", joint)
    if z >= 0.62:
        joint = "dist" if ax >= 22.05 else "prox"
        return ("middle", joint)
    if z >= 0.28:
        joint = "dist" if ax >= 22.05 else "prox"
        return ("ring", joint)
    if z < 0.28:
        joint = "dist" if ax >= 22.0 else "prox"
        return ("pinky", joint)
    return None


def classify_left(x: float, y: float, z: float, w: float) -> tuple[str, str] | None:
    return classify_digit(float(x), y, z, w)


def classify_right(x: float, y: float, z: float, w: float) -> tuple[str, str] | None:
    return classify_digit(-float(x), y, z, w)


def bone_local_origins(side: str) -> dict[str, tuple[list[float], float]]:
    """
    Hand-local origins for finger bones (parent = Hand).
    Left hand extends +X; right extends -X.
    Z spreads digits; Y slightly down for thumb.
    Curl axis: rotate around Z (left: -Z curls into palm for +X fingers; right mirrored).
    """
    # Proximal then distal origins relative to hand bone
    # Hand length is 3; tip is roughly at local +3 along bone. Place fingers near tip.
    sx = 1.0 if side == "Left" else -1.0
    # name_suffix -> (origin, length)
    return {
        "Thumb1": ([sx * 0.4, -0.15, 0.95], 0.85),
        "Thumb2": ([sx * 0.7, -0.05, 0.35], 0.7),  # child of Thumb1, relative
        "Index1": ([sx * 1.6, 0.0, 0.55], 0.9),
        "Index2": ([sx * 0.9, 0.0, 0.0], 0.75),
        "Middle1": ([sx * 1.7, 0.0, 0.15], 0.95),
        "Middle2": ([sx * 0.95, 0.0, 0.0], 0.8),
        "Ring1": ([sx * 1.55, 0.0, -0.25], 0.9),
        "Ring2": ([sx * 0.9, 0.0, 0.0], 0.75),
        "Pinky1": ([sx * 1.4, 0.0, -0.6], 0.8),
        "Pinky2": ([sx * 0.8, 0.0, 0.0], 0.65),
    }


DIGIT_TO_BONE = {
    ("thumb", "prox"): "Thumb1",
    ("thumb", "dist"): "Thumb2",
    ("index", "prox"): "Index1",
    ("index", "dist"): "Index2",
    ("middle", "prox"): "Middle1",
    ("middle", "dist"): "Middle2",
    ("ring", "prox"): "Ring1",
    ("ring", "dist"): "Ring2",
    ("pinky", "prox"): "Pinky1",
    ("pinky", "dist"): "Pinky2",
}


def animator_stub(name: str, bone_uuid: str, keyframes: list | None = None) -> tuple[str, dict]:
    d: dict = {
        "name": name,
        "type": "armature_bone",
        "rotation_global": False,
        "quaternion_interpolation": False,
    }
    if keyframes:
        d["keyframes"] = keyframes
    return bone_uuid, d


def rot_kf(time: float, x: float, y: float, z: float) -> dict:
    return {
        "channel": "rotation",
        "data_points": [{"x": str(x), "y": str(y), "z": str(z)}],
        "uuid": new_uuid(),
        "time": time,
        "color": -1,
        "interpolation": "linear",
    }


def main() -> None:
    raw = SRC.read_text(encoding="utf-8")
    data = json.loads(raw)
    if not BACKUP.exists():
        shutil.copy2(SRC, BACKUP)
        print(f"Backup: {BACKUP}")

    by_name = {e["name"]: e for e in data["elements"] if "name" in e}
    body = by_name[BODY_MESH]
    verts = body["vertices"]

    # Avoid double-adding if re-run
    if any(e.get("name") == "LeftThumb1" for e in data["elements"]):
        raise SystemExit("Finger bones already present — aborting to avoid duplicates.")

    created: list[dict] = []
    hand_children: dict[str, list[str]] = {"Left": [], "Right": []}
    bone_map: dict[str, dict] = {}  # "LeftThumb1" -> bone

    for side in ("Left", "Right"):
        hand = by_name[f"{side}Hand"]
        origins = bone_local_origins(side)
        color = 3 if side == "Left" else 1

        # Build proximal bones first, then distal as children
        prox_order = ["Thumb1", "Index1", "Middle1", "Ring1", "Pinky1"]
        dist_of = {
            "Thumb1": "Thumb2",
            "Index1": "Index2",
            "Middle1": "Middle2",
            "Ring1": "Ring2",
            "Pinky1": "Pinky2",
        }

        for prox in prox_order:
            dist = dist_of[prox]
            d_origin, d_len = origins[dist]
            dist_bone = make_bone(
                f"{side}{dist}",
                d_origin,
                d_len,
                color=color,
                connected=True,
            )
            p_origin, p_len = origins[prox]
            prox_bone = make_bone(
                f"{side}{prox}",
                p_origin,
                p_len,
                color=color,
                connected=False,
                children=[dist_bone["uuid"]],
            )
            created.extend([prox_bone, dist_bone])
            bone_map[prox_bone["name"]] = prox_bone
            bone_map[dist_bone["name"]] = dist_bone
            hand_children[side].append(prox_bone["uuid"])

        hand["children"] = hand_children[side]
        # Palm-focused hand length (was 3 covering whole hand)
        hand["length"] = 2.2

    # Reweight
    counts = {k: 0 for k in bone_map}
    counts["LeftHand"] = 0
    counts["RightHand"] = 0

    for side, classify in (("Left", classify_left), ("Right", classify_right)):
        hand = by_name[f"{side}Hand"]
        old_weights = dict(hand.get("vertex_weights") or {})
        new_hand_weights: dict[str, float] = {}
        for key, w in old_weights.items():
            # key like "5d1157:FXXz"
            if ":" not in key:
                new_hand_weights[key] = w
                continue
            prefix, vid = key.split(":", 1)
            if prefix != MESH_PREFIX or vid not in verts:
                new_hand_weights[key] = w
                continue
            x, y, z = verts[vid]
            hit = classify(float(x), float(y), float(z), float(w))
            if hit is None:
                new_hand_weights[key] = w
                counts[f"{side}Hand"] += 1
                continue
            digit, joint = hit
            bname = f"{side}{DIGIT_TO_BONE[(digit, joint)]}"
            bone = bone_map[bname]
            bone["vertex_weights"][key] = w
            counts[bname] = counts.get(bname, 0) + 1
        hand["vertex_weights"] = new_hand_weights

    # Insert new bones into elements (after hand bones)
    insert_at = None
    for i, e in enumerate(data["elements"]):
        if e.get("name") == "RightHand":
            insert_at = i + 1
            break
    if insert_at is None:
        insert_at = len(data["elements"])
    data["elements"][insert_at:insert_at] = created

    # Blockbench parenting is driven by outliner, not only element.children
    bone_map_by_uuid = {b["uuid"]: b for b in created}

    def hand_outliner_children(side: str) -> list[dict]:
        nodes = []
        for prox_uuid in hand_children[side]:
            prox = bone_map_by_uuid[prox_uuid]
            dist_nodes = [
                {"uuid": d, "isOpen": True, "children": []} for d in (prox.get("children") or [])
            ]
            nodes.append({"uuid": prox_uuid, "isOpen": True, "children": dist_nodes})
        return nodes

    left_out = hand_outliner_children("Left")
    right_out = hand_outliner_children("Right")
    left_hand_uuid = by_name["LeftHand"]["uuid"]
    right_hand_uuid = by_name["RightHand"]["uuid"]

    def patch_outliner(nodes: list) -> None:
        for n in nodes:
            if not isinstance(n, dict):
                continue
            if n.get("uuid") == left_hand_uuid:
                n["children"] = left_out
                n["isOpen"] = True
            elif n.get("uuid") == right_hand_uuid:
                n["children"] = right_out
                n["isOpen"] = True
            else:
                patch_outliner(n.get("children") or [])

    patch_outliner(data.get("outliner") or [])

    # Add Grip demo animation (open at 0, closed at 0.5, open at 1.0)
    # Curl: Left fingers rotate +Z (into palm for +X arm); Right fingers -Z.
    # Thumb: slight Y + X to oppose.
    animators: dict = {}
    for side in ("Left", "Right"):
        sx = 1.0 if side == "Left" else -1.0
        # finger curl around Z
        curl_closed = 55.0 * sx
        thumb_closed_y = -35.0 * sx
        thumb_closed_x = 25.0
        thumb2_closed = 40.0 * sx

        for digit, curl_scale in (
            ("Index", 1.0),
            ("Middle", 1.05),
            ("Ring", 0.95),
            ("Pinky", 0.9),
        ):
            for joint, jscale in (("1", 1.0), ("2", 1.15)):
                name = f"{side}{digit}{joint}"
                b = bone_map[name]
                z_closed = curl_closed * curl_scale * jscale
                animators[b["uuid"]] = {
                    "name": name,
                    "type": "armature_bone",
                    "rotation_global": False,
                    "quaternion_interpolation": False,
                    "keyframes": [
                        rot_kf(0.0, 0, 0, 0),
                        rot_kf(0.5, 0, 0, z_closed),
                        rot_kf(1.0, 0, 0, 0),
                    ],
                }

        t1 = bone_map[f"{side}Thumb1"]
        t2 = bone_map[f"{side}Thumb2"]
        animators[t1["uuid"]] = {
            "name": t1["name"],
            "type": "armature_bone",
            "rotation_global": False,
            "quaternion_interpolation": False,
            "keyframes": [
                rot_kf(0.0, 0, 0, 0),
                rot_kf(0.5, thumb_closed_x, thumb_closed_y, 15.0 * sx),
                rot_kf(1.0, 0, 0, 0),
            ],
        }
        animators[t2["uuid"]] = {
            "name": t2["name"],
            "type": "armature_bone",
            "rotation_global": False,
            "quaternion_interpolation": False,
            "keyframes": [
                rot_kf(0.0, 0, 0, 0),
                rot_kf(0.5, 10.0, 0, thumb2_closed),
                rot_kf(1.0, 0, 0, 0),
            ],
        }

    grip = {
        "uuid": new_uuid(),
        "name": "HandGrip",
        "loop": "loop",
        "override": False,
        "length": 1.0,
        "snapping": 20,
        "selected": False,
        "anim_time_update": "",
        "blend_weight": "",
        "start_delay": "",
        "loop_delay": "",
        "animators": animators,
    }
    data.setdefault("animations", []).append(grip)

    out = json.dumps(data, separators=(",", ":"))
    SRC.write_text(out, encoding="utf-8")
    REPO_COPY.write_text(out, encoding="utf-8")

    print("Reweight counts:")
    for k in sorted(counts):
        print(f"  {k}: {counts[k]}")
    print(f"Wrote {SRC}")
    print(f"Wrote {REPO_COPY}")
    print(f"Added bones: {len(created)}; animation HandGrip")


if __name__ == "__main__":
    main()
