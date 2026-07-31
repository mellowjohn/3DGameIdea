"""Export open GoodPlayerModel from Blockbench HTTP MCP → tools/art/player."""
from __future__ import annotations

import json
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools" / "art" / "player"))
from _bb_mcp_client import BlockbenchMcp  # noqa: E402

OUT_GLTF = REPO / "tools/art/player/GoodPlayerModel.gltf"
OUT_BB = REPO / "tools/art/player/GoodPlayerModel_rigged.bbmodel"
DOCS_BB = Path(r"C:\Users\johnr\Documents\GoodPlayerModel.bbmodel")
DOCS_PNG = Path(r"C:\Users\johnr\Documents\GoodPlayerModel.png")


def main() -> int:
    bb = BlockbenchMcp()
    print("connecting...", bb.connect())
    result = bb.call_tool(
        "export_model",
        {
            "codec_id": "gltf",
            "path": str(OUT_GLTF),
            "max_content_length": 0,
            "options": {"encoding": "embed"},
        },
    )
    print("export:", result if isinstance(result, str) else json.dumps(result)[:1500])
    if DOCS_BB.exists():
        shutil.copy2(DOCS_BB, OUT_BB)
        print("copied bbmodel", OUT_BB)
    if DOCS_PNG.exists():
        shutil.copy2(DOCS_PNG, REPO / "tools/art/player/GoodPlayerModel.png")
        print("copied png")
    if not OUT_GLTF.exists():
        print("ERROR: glTF not written", OUT_GLTF)
        return 1
    g = json.loads(OUT_GLTF.read_text(encoding="utf-8"))
    clips = [a.get("name") for a in g.get("animations") or []]
    print("clips:", clips)
    print("size:", OUT_GLTF.stat().st_size)
    return 0 if clips else 1


if __name__ == "__main__":
    raise SystemExit(main())
