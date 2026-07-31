"""Inspect GoodPlayerModel bbmodel rig hierarchy/weights."""
from __future__ import annotations

import json
from pathlib import Path


def analyze(path: Path) -> None:
    data = json.loads(path.read_text(encoding="utf-8"))
    els = {e["uuid"]: e for e in data.get("elements") or []}
    bones = [e for e in els.values() if e.get("type") == "armature_bone"]
    meshes = [e for e in els.values() if e.get("type") == "mesh"]
    byname = {b["name"]: b for b in bones}

    parent: dict[str, str | None] = {}

    def walk(nodes, pid=None):
        for n in nodes or []:
            if isinstance(n, str):
                parent[n] = pid
            elif isinstance(n, dict):
                uid = n.get("uuid")
                parent[uid] = pid
                walk(n.get("children"), uid)

    walk(data.get("outliner"))

    print("FILE", path)
    print(" bytes", path.stat().st_size, "bones", len(bones), "meshes", len(meshes))
    arms = [
        "LeftUpperArm",
        "LeftLowerArm",
        "LeftHand",
        "RightUpperArm",
        "RightLowerArm",
        "RightHand",
    ]
    for n in arms:
        b = byname.get(n)
        if not b:
            print(" missing", n)
            continue
        pid = parent.get(b["uuid"])
        pname = els.get(pid, {}).get("name") if pid else None
        vw = len(b.get("vertex_weights") or {})
        print(f"  {n}: parent={pname} origin={b.get('origin')} weights={vw}")

    for mn in ["LPalm", "RPalm", "LWristSeam", "BodyMesh", "LIndex1"]:
        ms = [m for m in meshes if m.get("name") == mn]
        for m in ms:
            pid = parent.get(m["uuid"])
            if pid is None:
                pname = "OUTLINER_MISSING"
            elif pid in els:
                pname = els[pid].get("name")
            else:
                pname = f"unknown:{pid[:8]}"
            print(f"  mesh {mn}: parent={pname} origin={m.get('origin')}")

    tw = sum(len(b.get("vertex_weights") or {}) for b in bones)
    print(" totalWeightEntries", tw)
    posed = [
        b["name"]
        for b in bones
        if any(abs(x) > 1e-6 for x in (b.get("rotation") or [0, 0, 0]))
    ]
    print(" posedBones", posed)

    # BodyMesh uuid prefix weights
    body = next((m for m in meshes if m["name"] == "BodyMesh"), None)
    if body:
        pref = body["uuid"].replace("-", "")[:6]
        body_w = 0
        for b in bones:
            for k in b.get("vertex_weights") or {}:
                if k.startswith(pref + ":"):
                    body_w += 1
        print(" bodyUuid", body["uuid"], "prefix", pref, "bodyWeightKeys", body_w)


if __name__ == "__main__":
    analyze(Path(r"C:/Users/johnr/Documents/GoodPlayerModel.bbmodel"))
    print()
    analyze(Path(r"C:/Users/johnr/Documents/GoodPlayerModel_rigged.bbmodel"))
