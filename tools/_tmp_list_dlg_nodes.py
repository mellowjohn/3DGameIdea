import json
from pathlib import Path

p = Path("samples/open-world-rpg/assets/world-forge/dialogues.worldforge.json")
d = json.loads(p.read_text(encoding="utf-8"))
tree = next(t for t in d["trees"] if t["id"] == "dlg_act0_wrathful_conquest")
print("nodes", len(tree["nodes"]))
print("entry", tree["entryNodeId"])
ids = {n["id"] for n in tree["nodes"]}
for n in tree["nodes"]:
    line = (n.get("line") or "").replace("\n", " ")[:80]
    ch = ",".join(c.get("nextNodeId") or "END" for c in n.get("choices", []))
    print(f"{n['id']}\tsp={n.get('speakerId','')}\tch={len(n.get('choices',[]))}\t->{ch}\t{line}")
