import json
from pathlib import Path


def inspect(path: Path) -> None:
    d = json.loads(path.read_text(encoding="utf-8"))
    print("===", path)
    print("meshes", len(d.get("meshes") or []), [m.get("name") for m in d.get("meshes") or []])
    mesh_nodes = [
        (i, n.get("name"), "skin" in n)
        for i, n in enumerate(d.get("nodes") or [])
        if "mesh" in n
    ]
    print("meshNodes", mesh_nodes)
    for mi, m in enumerate(d.get("meshes") or []):
        for pi, p in enumerate(m.get("primitives") or []):
            acc = d["accessors"][p["attributes"]["POSITION"]]
            print(
                f"  mesh{mi}.{pi} name={m.get('name')} verts={acc['count']} "
                f"attrs={sorted(p['attributes'].keys())}"
            )
    skins = d.get("skins") or []
    if skins:
        joints = [d["nodes"][i].get("name") for i in skins[0].get("joints") or []]
        handish = [j for j in joints if any(k in j for k in ("Hand", "Thumb", "Index", "Middle", "Ring", "Pinky"))]
        print("handJoints", len(handish), handish[:12])


inspect(Path(r"C:/Users/johnr/Documents/Models/GoodPlayerModel.gltf"))
print()
inspect(Path(r"C:/Users/johnr/Documents/3DGameIdea/samples/open-world-rpg/assets/models/player.gltf"))
print()

bb_path = Path(r"C:/Users/johnr/Documents/GoodPlayerModel.bbmodel")
bb = json.loads(bb_path.read_text(encoding="utf-8"))
meshes = [e for e in (bb.get("elements") or []) if e.get("type") == "mesh"]
print("bb meshes", len(meshes))
for m in sorted(meshes, key=lambda x: x.get("name", "")):
    name = m.get("name", "?")
    print(f"  {name}: verts={len(m.get('vertices') or {})} faces={len(m.get('faces') or {})}")
