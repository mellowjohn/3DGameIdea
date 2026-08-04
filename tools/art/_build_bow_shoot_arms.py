"""Rewrite player.BowShoot.anim.json arm poses with correct sagittal channel roles.

Engine samples player clips with RH→LH sagittal remapping:
  joint Left* (visual right / bow hand with LeftHand weld) ← clip channels Right*
  joint Right* (visual left / string hand) ← clip channels Left*

Author pose vocabulary as on-screen roles, then write mirrored names into the override.
"""
from __future__ import annotations

import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "samples/open-world-rpg/assets/models/player.BowShoot.anim.json"

# Idle local rests from player.gltf Idle clip (blockbench axis).
IDLE = {
    "Hips_r": (0.0, 0.0, 0.0, 1.0),
    "Hips_t": (0.0, 1.123882532119751, 0.03512132912874222),
    "Spine_r": (-0.01308959536254406, 0.0, 0.0, 0.9999143481254578),
    "Chest_r": (-0.017452405765652657, 0.0, 0.0, 0.9998477101325989),
    "Chest_t": (0.0, 0.49169859290122986, 0.0),
    "Neck_r": (0.008726535364985466, 0.0, 0.0, 0.9999619126319885),
    "Head_r": (0.017446426674723625, -0.02617296203970909, -0.0004568507429212332, 0.9995050430297852),
    "Head_t": (0.0, 0.1756066381931305, -0.04214559495449066),
    # File-space names (Blockbench Right = visual right after sagittal).
    "RightUpperArm_r": (0.04522397741675377, 0.05587825924158096, -0.5992853045463562, 0.7973014116287231),
    "RightUpperArm_t": (0.38107553124427795, 0.13755831122398376, 0.004683077801018953),
    "RightLowerArm_r": (-0.08564168959856033, -0.030601415783166885, -0.049840591847896576, 0.9946079254150391),
    "RightHand_r": (0.0, 0.0, -0.03489949554204941, 0.9993908405303955),
    "LeftUpperArm_r": (0.03130226209759712, -0.04541077837347984, 0.6001692414283752, 0.7979692220687866),
    "LeftUpperArm_t": (-0.38107553124427795, 0.13755831122398376, 0.004683077801018953),
    "LeftLowerArm_r": (-0.06873910129070282, 0.02103624865412712, 0.05098476633429527, 0.996108889579773),
    "LeftHand_r": (0.0, 0.0, 0.03489949554204941, 0.9993908405303955),
    "LeftUpperLeg_r": None,  # keep body-only below as identity-ish from prior clip if needed
    "RightUpperLeg_r": None,
}

# Node indices from runtime player.gltf
NODE = {
    "Head": 0,
    "RightHand": 12,
    "RightLowerArm": 13,
    "RightUpperArm": 14,
    "LeftHand": 25,
    "LeftLowerArm": 26,
    "LeftUpperArm": 27,
    "Chest": 28,
    "Hips": 36,
    "Spine": 29,  # may be wrong — detect below
    "Neck": 1,
}


def _load_node_indices() -> None:
    g = json.loads((ROOT / "samples/open-world-rpg/assets/models/player.gltf").read_text(encoding="utf-8"))
    for i, n in enumerate(g["nodes"]):
        name = n.get("name")
        if name in NODE:
            NODE[name] = i
        if name == "Spine":
            NODE["Spine"] = i
        if name == "Neck":
            NODE["Neck"] = i


def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    )


def qnorm(q):
    x, y, z, w = q
    n = math.sqrt(x * x + y * y + z * z + w * w) or 1.0
    return (x / n, y / n, z / n, w / n)


def euler_xyz_deg(rx, ry, rz):
    """Local XYZ euler degrees → quat xyzw."""
    rx, ry, rz = map(math.radians, (rx, ry, rz))
    cx, sx = math.cos(rx / 2), math.sin(rx / 2)
    cy, sy = math.cos(ry / 2), math.sin(ry / 2)
    cz, sz = math.cos(rz / 2), math.sin(rz / 2)
    # XYZ intrinsic
    return qnorm(
        (
            sx * cy * cz - cx * sy * sz,
            cx * sy * cz + sx * cy * sz,
            cx * cy * sz - sx * sy * cz,
            cx * cy * cz + sx * sy * sz,
        )
    )


def apply_delta(base, rx, ry, rz):
    return qnorm(qmul(base, euler_xyz_deg(rx, ry, rz)))


# Timeline (seconds) — pull → full draw plateau → release → settle
TIMES_ARM = [0.0, 0.18, 0.45, 0.55, 1.15, 1.35, 1.52, 1.7]
# Spine/chest fewer keys same envelope
TIMES_BODY = [0.0, 0.18, 0.45, 1.15, 1.35, 1.52, 1.7]
TIMES_HIPS = [0.0, 0.85, 1.7]


def arm_pose_table():
    """Deltas (deg) applied to Idle for [each TIMES_ARM key].

    File Right* = BOW hand (visual right). File Left* = STRING (visual left).
    """
    # BOW upper: raise slightly, push forward across body, less abduced than idle
    bow_ua = [
        (0, 0, 0),
        (-8, 8, 12),  # start raise / bring in
        (-18, 18, 22),  # mid hold
        (-22, 22, 26),  # full aim
        (-22, 22, 26),
        (-12, 10, 14),  # release soften
        (-4, 4, 6),
        (0, 0, 0),
    ]
    # BOW lower: mild bend so hand stays ahead of chest, not a hard draw elbow
    bow_la = [
        (0, 0, 0),
        (12, -4, 2),
        (28, -8, 4),
        (34, -10, 4),
        (34, -10, 4),
        (16, -4, 2),
        (6, -2, 0),
        (0, 0, 0),
    ]
    bow_h = [
        (0, 0, 0),
        (6, -6, -8),
        (12, -12, -16),
        (14, -14, -18),
        (14, -14, -18),
        (6, -6, -8),
        (2, -2, -2),
        (0, 0, 0),
    ]

    # STRING upper: abduce back, elbow high (draw)
    str_ua = [
        (0, 0, 0),
        (-18, -6, -18),
        (-38, -4, -32),
        (-48, -2, -38),  # deep draw
        (-48, -2, -38),
        (8, 4, -8),  # snap forward on release
        (4, 2, -4),
        (0, 0, 0),
    ]
    # STRING lower: deep elbow fold into full draw
    str_la = [
        (0, 0, 0),
        (35, -6, 4),
        (72, -12, 6),
        (95, -14, 4),
        (95, -14, 4),
        (18, -4, 0),  # open elbow on loose
        (4, -2, 0),
        (0, 0, 0),
    ]
    str_h = [
        (0, 0, 0),
        (10, 6, 10),
        (22, 10, 18),
        (28, 12, 22),
        (28, 12, 22),
        (6, 2, 4),
        (0, 0, 0),
        (0, 0, 0),
    ]

    # Map: visual roles → FILE channel names (opposite of joint names under sagittal)
    # Bow visual right → Right* file channels
    # String visual left → Left* file channels
    return {
        "RightUpperArm": bow_ua,
        "RightLowerArm": bow_la,
        "RightHand": bow_h,
        "LeftUpperArm": str_ua,
        "LeftLowerArm": str_la,
        "LeftHand": str_h,
    }


def body_pose_table():
    # mild torso toward bow side / aim
    spine = [
        (0, 0, 0),
        (2, -6, 1),
        (5, -12, 2),
        (5, -12, 2),
        (2, -5, 1),
        (0, -2, 0),
        (0, 0, 0),
    ]
    chest = [
        (0, 0, 0),
        (1, -4, 1),
        (3, -8, 2),
        (3, -8, 2),
        (1, -3, 1),
        (0, -1, 0),
        (0, 0, 0),
    ]
    neck = [
        (0, 0, 0),
        (0, 3, 0),
        (0, 6, 0),
        (0, 6, 0),
        (0, 2, 0),
        (0, 0, 0),
        (0, 0, 0),
    ]
    head = [
        (0, 0, 0),
        (0, 4, 0),
        (2, 8, 0),
        (2, 8, 0),
        (0, 3, 0),
        (0, 0, 0),
        (0, 0, 0),
    ]
    return spine, chest, neck, head


def rot_channel(name, times, base, deltas):
    vals = []
    for d in deltas:
        q = apply_delta(base, *d)
        vals.extend(q)
    return {
        "targetNodeName": name,
        "targetNodeIndex": NODE[name],
        "path": "rotation",
        "interpolation": "LINEAR",
        "times": list(times),
        "values": vals,
    }


def tr_channel(name, times, values_xyz_flat):
    return {
        "targetNodeName": name,
        "targetNodeIndex": NODE[name],
        "path": "translation",
        "interpolation": "LINEAR",
        "times": list(times),
        "values": list(values_xyz_flat),
    }


def main() -> None:
    _load_node_indices()
    arms = arm_pose_table()
    channels = []

    # Arms from Idle base + deltas
    for joint, deltas in arms.items():
        base = IDLE[f"{joint}_r"]
        channels.append(rot_channel(joint, TIMES_ARM, base, deltas))

    # Keep upper-arm translations near Idle (shoulders don’t float)
    for joint in ("RightUpperArm", "LeftUpperArm"):
        t = IDLE[f"{joint}_t"]
        channels.append(tr_channel(joint, [0.0, 0.85, 1.7], [*t, *t, *t]))

    spine_d, chest_d, neck_d, head_d = body_pose_table()
    channels.append(rot_channel("Spine", TIMES_BODY, IDLE["Spine_r"], spine_d))
    channels.append(rot_channel("Chest", TIMES_BODY, IDLE["Chest_r"], chest_d))
    channels.append(rot_channel("Neck", TIMES_BODY, IDLE["Neck_r"], neck_d))
    channels.append(rot_channel("Head", TIMES_BODY, IDLE["Head_r"], head_d))

    # Slight chest translation pulse (forward)
    ct = IDLE["Chest_t"]
    channels.append(
        tr_channel(
            "Chest",
            [0.0, 0.45, 1.15, 1.7],
            [
                ct[0],
                ct[1],
                ct[2],
                ct[0],
                ct[1] + 0.02,
                ct[2] + 0.01,
                ct[0],
                ct[1] + 0.02,
                ct[2] + 0.01,
                ct[0],
                ct[1],
                ct[2],
            ],
        )
    )
    channels.append(
        tr_channel("Head", [0.0, 0.85, 1.7], [*IDLE["Head_t"], *IDLE["Head_t"], *IDLE["Head_t"]])
    )

    channels.append(rot_channel("Hips", TIMES_HIPS, IDLE["Hips_r"], [(0, 0, 0), (0, -4, 0), (0, 0, 0)]))
    ht = IDLE["Hips_t"]
    channels.append(tr_channel("Hips", TIMES_HIPS, [*ht, ht[0], ht[1], ht[2] + 0.01, *ht]))

    # Legs stay Idle-stable if we omit; prior clip keyed upper legs at rest — leave out

    out = {
        "schemaVersion": 1,
        "kind": "animationClipOverride",
        "clipSource": "assets/models/player.gltf",
        "clipName": "BowShoot",
        "durationSeconds": 1.7,
        "channels": channels,
    }
    OUT.write_text(json.dumps(out, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {OUT} channels={len(channels)}")
    print("ROLE: Right* = bow (visual right), Left* = string (visual left) under sagittal sampling")


if __name__ == "__main__":
    main()
