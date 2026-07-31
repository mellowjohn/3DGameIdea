# Build Run animation for GoodPlayerModel via Blockbench MCP create_animation.
# Values adapted from Player_V2 Run; arms simplified for T-pose bind.

LENGTH = 0.8

def k(time, rotation=None, position=None):
    d = {"time": time}
    if rotation is not None:
        d["rotation"] = list(rotation)
    if position is not None:
        d["position"] = list(position)
    return d


bones = {
    "Hips": [
        k(0.0, rotation=(-8, -4, 2), position=(0, -1.1, 0.1)),
        k(0.2, rotation=(-4, 0, 0), position=(0, 0.2, 0.2)),
        k(0.3, rotation=(-2, 4, 0), position=(0, 1.4, 0.25)),
        k(0.4, rotation=(-6, 6, 0), position=(0, -0.2, 0.15)),
        k(0.5, rotation=(-8, 4, -2), position=(0, -1.1, 0.1)),
        k(0.6, rotation=(-4, 0, 0), position=(0, 0.2, 0.2)),
        k(0.7, rotation=(-2, -4, 0), position=(0, 1.4, 0.25)),
        k(0.8, rotation=(-8, -4, 2), position=(0, -1.1, 0.1)),
    ],
    "Spine": [
        k(0.0, rotation=(-10, 2, 1)),
        k(0.3, rotation=(-7, -2, 0)),
        k(0.4, rotation=(-8, -3, 0)),
        k(0.5, rotation=(-10, -2, -1)),
        k(0.7, rotation=(-7, 2, 0)),
        k(0.8, rotation=(-10, 2, 1)),
    ],
    "Chest": [
        k(0.0, rotation=(-14, 5, 2)),
        k(0.3, rotation=(-10, -4, 0)),
        k(0.4, rotation=(-12, -8, 0)),
        k(0.5, rotation=(-14, -5, -2)),
        k(0.7, rotation=(-10, 4, 0)),
        k(0.8, rotation=(-14, 5, 2)),
    ],
    "Neck": [
        k(0.0, rotation=(18, -6, 0)),
        k(0.2, rotation=(19, 0, 0)),
        k(0.4, rotation=(18, 6, 0)),
        k(0.6, rotation=(19, 0, 0)),
        k(0.8, rotation=(18, -6, 0)),
    ],
    "Head": [
        k(0.0, rotation=(4, 3, -2)),
        k(0.4, rotation=(4, -3, 2)),
        k(0.8, rotation=(4, 3, -2)),
    ],
    # Arms from T-pose hang (~Z ±74); swing opposite to legs
    "LeftUpperArm": [
        k(0.0, rotation=(-35, -8, -70)),   # forward (left leg back)
        k(0.2, rotation=(-10, -4, -74)),
        k(0.4, rotation=(25, 5, -78)),     # back (left leg forward)
        k(0.6, rotation=(-5, -2, -74)),
        k(0.8, rotation=(-35, -8, -70)),
    ],
    "LeftLowerArm": [
        k(0.0, rotation=(55, 10, -25)),
        k(0.4, rotation=(75, 5, -15)),
        k(0.8, rotation=(55, 10, -25)),
    ],
    "LeftHand": [
        k(0.0, rotation=(-15, 5, -10)),
        k(0.4, rotation=(-15, -5, -10)),
        k(0.8, rotation=(-15, 5, -10)),
    ],
    "RightUpperArm": [
        k(0.0, rotation=(25, -5, 78)),     # back (right leg forward)
        k(0.2, rotation=(-5, 2, 74)),
        k(0.4, rotation=(-35, 8, 70)),     # forward
        k(0.6, rotation=(-10, 4, 74)),
        k(0.8, rotation=(25, -5, 78)),
    ],
    "RightLowerArm": [
        k(0.0, rotation=(75, -5, 15)),
        k(0.4, rotation=(55, -10, 25)),
        k(0.8, rotation=(75, -5, 15)),
    ],
    "RightHand": [
        k(0.0, rotation=(-15, -5, 10)),
        k(0.4, rotation=(-15, 5, 10)),
        k(0.8, rotation=(-15, -5, 10)),
    ],
    # Legs from V2
    "LeftUpperLeg": [
        k(0.0, rotation=(-32, 5, -3)),
        k(0.1, rotation=(-22, 3, -2)),
        k(0.2, rotation=(15, 0, 0)),
        k(0.3, rotation=(48, 3, -2)),
        k(0.4, rotation=(42, 5, -3)),
        k(0.5, rotation=(28, 4, -2)),
        k(0.6, rotation=(8, 2, -1)),
        k(0.7, rotation=(-18, 0, 0)),
        k(0.8, rotation=(-32, 5, -3)),
    ],
    "LeftLowerLeg": [
        k(0.0, rotation=(-72, 0, 0)),
        k(0.1, rotation=(-85, 0, 0)),
        k(0.2, rotation=(-95, 0, 0)),
        k(0.3, rotation=(-70, 0, 0)),
        k(0.4, rotation=(-18, 0, 0)),
        k(0.5, rotation=(-48, 0, 0)),
        k(0.6, rotation=(-12, 0, 0)),
        k(0.7, rotation=(-55, 0, 0)),
        k(0.8, rotation=(-72, 0, 0)),
    ],
    "LeftFoot": [
        k(0.0, rotation=(25, 0, 0)),
        k(0.2, rotation=(10, 0, 0)),
        k(0.3, rotation=(-5, 0, 0)),
        k(0.4, rotation=(-12, 0, 0)),
        k(0.5, rotation=(-28, 0, 0)),
        k(0.6, rotation=(5, 0, 0)),
        k(0.7, rotation=(15, 0, 0)),
        k(0.8, rotation=(25, 0, 0)),
    ],
    "RightUpperLeg": [
        k(0.0, rotation=(42, -5, 3)),
        k(0.1, rotation=(28, -4, 2)),
        k(0.2, rotation=(8, -2, 1)),
        k(0.3, rotation=(-18, 0, 0)),
        k(0.4, rotation=(-32, 4, -2)),
        k(0.5, rotation=(-22, 3, -2)),
        k(0.6, rotation=(15, 0, 0)),
        k(0.7, rotation=(48, -3, 2)),
        k(0.8, rotation=(42, -5, 3)),
    ],
    "RightLowerLeg": [
        k(0.0, rotation=(-18, 0, 0)),
        k(0.1, rotation=(-48, 0, 0)),
        k(0.2, rotation=(-12, 0, 0)),
        k(0.3, rotation=(-55, 0, 0)),
        k(0.4, rotation=(-72, 0, 0)),
        k(0.5, rotation=(-85, 0, 0)),
        k(0.6, rotation=(-95, 0, 0)),
        k(0.7, rotation=(-70, 0, 0)),
        k(0.8, rotation=(-18, 0, 0)),
    ],
    "RightFoot": [
        k(0.0, rotation=(-12, 0, 0)),
        k(0.1, rotation=(-28, 0, 0)),
        k(0.2, rotation=(5, 0, 0)),
        k(0.3, rotation=(15, 0, 0)),
        k(0.4, rotation=(25, 0, 0)),
        k(0.6, rotation=(10, 0, 0)),
        k(0.7, rotation=(-5, 0, 0)),
        k(0.8, rotation=(-12, 0, 0)),
    ],
}

# Fist hold (static across cycle)
fist = {
    "LeftThumb1": [k(0.0, rotation=(25, -35, 15)), k(0.8, rotation=(25, -35, 15))],
    "LeftThumb2": [k(0.0, rotation=(10, 0, 40)), k(0.8, rotation=(10, 0, 40))],
    "LeftIndex1": [k(0.0, rotation=(0, 0, 55)), k(0.8, rotation=(0, 0, 55))],
    "LeftIndex2": [k(0.0, rotation=(0, 0, 63)), k(0.8, rotation=(0, 0, 63))],
    "LeftMiddle1": [k(0.0, rotation=(0, 0, 58)), k(0.8, rotation=(0, 0, 58))],
    "LeftMiddle2": [k(0.0, rotation=(0, 0, 66)), k(0.8, rotation=(0, 0, 66))],
    "LeftRing1": [k(0.0, rotation=(0, 0, 52)), k(0.8, rotation=(0, 0, 52))],
    "LeftRing2": [k(0.0, rotation=(0, 0, 60)), k(0.8, rotation=(0, 0, 60))],
    "LeftPinky1": [k(0.0, rotation=(0, 0, 50)), k(0.8, rotation=(0, 0, 50))],
    "LeftPinky2": [k(0.0, rotation=(0, 0, 57)), k(0.8, rotation=(0, 0, 57))],
    "RightThumb1": [k(0.0, rotation=(25, 35, -15)), k(0.8, rotation=(25, 35, -15))],
    "RightThumb2": [k(0.0, rotation=(10, 0, -40)), k(0.8, rotation=(10, 0, -40))],
    "RightIndex1": [k(0.0, rotation=(0, 0, -55)), k(0.8, rotation=(0, 0, -55))],
    "RightIndex2": [k(0.0, rotation=(0, 0, -63)), k(0.8, rotation=(0, 0, -63))],
    "RightMiddle1": [k(0.0, rotation=(0, 0, -58)), k(0.8, rotation=(0, 0, -58))],
    "RightMiddle2": [k(0.0, rotation=(0, 0, -66)), k(0.8, rotation=(0, 0, -66))],
    "RightRing1": [k(0.0, rotation=(0, 0, -52)), k(0.8, rotation=(0, 0, -52))],
    "RightRing2": [k(0.0, rotation=(0, 0, -60)), k(0.8, rotation=(0, 0, -60))],
    "RightPinky1": [k(0.0, rotation=(0, 0, -50)), k(0.8, rotation=(0, 0, -50))],
    "RightPinky2": [k(0.0, rotation=(0, 0, -57)), k(0.8, rotation=(0, 0, -57))],
}
bones.update(fist)

import json
from pathlib import Path
Path(r"C:/Users/johnr/Documents/3DGameIdea/tools/art/player/_run_keyframes.json").write_text(
    json.dumps({"name": "Run", "loop": True, "animation_length": LENGTH, "bones": bones}, indent=2),
    encoding="utf-8",
)
print("bones", len(bones), "keys", sum(len(v) for v in bones.values()))
