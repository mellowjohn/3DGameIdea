import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _bb_mcp_client import BlockbenchMcp

payload = json.loads(
    Path(__file__).with_name("_run_keyframes.json").read_text(encoding="utf-8")
)
client = BlockbenchMcp()
client.connect()
removed = client.eval(
    "(() => { const hits = Animation.all.filter(a => a.name.toLowerCase().indexOf('run') >= 0); "
    "hits.forEach(a => a.remove(false)); return {removed: hits.length}; })()"
)
print("removed", removed)
out = client.call_tool("create_animation", payload)
print("created", out)
fix = client.eval(
    "(() => { const a = Animation.all.find(x => x.name.toLowerCase().indexOf('run') >= 0); "
    "if (!a) return {error: 'missing'}; a.name = 'Run'; a.loop = 'loop'; a.length = 0.8; a.select(); "
    "Timeline.setTime(0.2); Animator.preview(true); Canvas.updateAllBones(); "
    "try { Animator.displayMeshDeformation(); } catch (e) {} Preview.selected.render(); "
    "function world(n){ const b = ArmatureBone.all.find(x => x.name === n); "
    "const wp = new THREE.Vector3(); b.mesh.getWorldPosition(wp); return wp.toArray().map(v => +v.toFixed(2)); } "
    "const keyTotal = Object.values(a.animators||{}).reduce((n, an) => "
    "n + ((an.rotation||[]).length + (an.position||[]).length), 0); "
    "return {name:a.name, length:a.length, loop:a.loop, keyTotal, "
    "leftHand: world('LeftHand'), rightHand: world('RightHand'), "
    "leftFoot: world('LeftFoot'), rightFoot: world('RightFoot'), hips: world('Hips')}; })()"
)
print(json.dumps(fix, indent=2))
