import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _bb_mcp_client import BlockbenchMcp

client = BlockbenchMcp()
client.connect()
code = Path(__file__).with_name("_fix_run.js").read_text(encoding="utf-8")
result = client.eval(code)
print(json.dumps(result, indent=2))

save = client.eval(
    r"""(() => {
  const fs = require('fs');
  const data = Codecs.project.compile({raw: true});
  const json = typeof data === 'string' ? data : JSON.stringify(data);
  for (const p of [
    'C:/Users/johnr/Documents/GoodPlayerModel.bbmodel',
    'C:/Users/johnr/Documents/GoodPlayerModel_rigged.bbmodel',
    'C:/Users/johnr/Documents/3DGameIdea/tools/art/player/GoodPlayerModel_rigged.bbmodel'
  ]) fs.writeFileSync(p, json);
  Timeline.setTime(0.2);
  Animator.preview(true);
  Canvas.updateAllBones();
  try { Animator.displayMeshDeformation(); } catch (e) {}
  Preview.selected.render();
  return {bytes: json.length, anims: Animation.all.map(a => a.name)};
})()"""
)
print("saved", save)
