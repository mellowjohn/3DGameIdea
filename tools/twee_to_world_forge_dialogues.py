#!/usr/bin/env python3
"""Convert Twine Act 0 (.twee / Harlowe) into dialogues.worldforge.json (TICKET-0052).

Source of truth for Act 0 main-quest lines: context/story/sources/wrathful-conquest-act0.twee
Canon labeling stays draft until beat-sheet review.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

LINK_RE = re.compile(
    r"\[\[(?:\"([^\"]+)\"|([^\]|]+))(?:\|([^\]]+))?\]\]"
)
HEADER_RE = re.compile(r'^::\s*(.+?)(?:\s+(\{.*\}))?\s*$')


def slugify(title: str) -> str:
    s = title.strip().lower()
    s = re.sub(r"[^a-z0-9]+", "_", s)
    s = s.strip("_")
    return s or "node"


def parse_twee(text: str) -> dict[str, dict]:
    passages: dict[str, dict] = {}
    current_title: str | None = None
    body_lines: list[str] = []

    def flush() -> None:
        nonlocal current_title, body_lines
        if current_title is None:
            return
        if current_title in ("StoryTitle", "StoryData"):
            current_title = None
            body_lines = []
            return
        # Strip Twine position metadata suffix already handled in header.
        body = "\n".join(body_lines).strip()
        passages[current_title] = {"title": current_title, "body": body}
        current_title = None
        body_lines = []

    for line in text.splitlines():
        m = HEADER_RE.match(line)
        if m:
            flush()
            title = m.group(1).strip()
            # Titles may start with `\ "` for escaped choice passages.
            if title.startswith("\\"):
                title = title[1:].lstrip()
            if title.startswith('"') and title.endswith('"'):
                title = title[1:-1]
            current_title = title
            continue
        if current_title is not None:
            body_lines.append(line)
    flush()
    return passages


def extract_links(body: str) -> tuple[str, list[tuple[str, str]]]:
    """Return (line_text_without_links, [(choice_text, target_title), ...])."""
    choices: list[tuple[str, str]] = []
    for m in LINK_RE.finditer(body):
        quoted, plain, target = m.group(1), m.group(2), m.group(3)
        label = (quoted or plain or "").strip()
        dest = (target or label).strip()
        if dest.startswith('"') and dest.endswith('"'):
            dest = dest[1:-1]
        choices.append((label, dest))
    cleaned = LINK_RE.sub("", body)
    # Drop OOC stage directions from spoken line; keep as summary-ish body.
    cleaned = re.sub(r"\(\(OOC[^)]*\)\)", "", cleaned)
    cleaned = re.sub(r"\(OOC[^)]*\)\)?", "", cleaned)
    cleaned = re.sub(r"\n{3,}", "\n\n", cleaned).strip()
    return cleaned, choices


def infer_speaker(line: str, title: str) -> str:
    lower = (line + " " + title).lower()
    if "creotar" in lower and ("whisper" in lower or "silhouette" in lower or "my name" in lower):
        return "creotar"
    if "commander" in lower or "grenge" in lower:
        return "commander_grenge"
    if "arkand" in lower and line.count('"') >= 2:
        # Prefer arkand when he has quoted speech early in the passage set.
        pass
    if re.search(r'Arkand[^"]*states|"[^"]*"\s*\n\s*Arkand|Arkand.*replies|Arkand Shouts', line):
        return "arkand"
    if "larrell" in lower:
        return "sergeant_larrell"
    if title in ("Prologue",):
        return "narrator"
    if "Realm of Darkness" in title or "Frangitur" in title:
        return "frangitur"
    if title.startswith("Tutorial"):
        return "arkand" if "arkand" in lower else "narrator"
    return "narrator"


def convert(twee_path: Path) -> dict:
    passages = parse_twee(twee_path.read_text(encoding="utf-8"))
    title_to_id = {title: slugify(title) for title in passages}

    # Disambiguate collisions.
    seen: dict[str, int] = {}
    for title, nid in list(title_to_id.items()):
        if nid in seen:
            seen[nid] += 1
            title_to_id[title] = f"{nid}_{seen[nid]}"
        else:
            seen[nid] = 1

    nodes = []
    for title, data in passages.items():
        line, links = extract_links(data["body"])
        node_id = title_to_id[title]
        choices = []
        for i, (label, dest) in enumerate(links):
            target_id = title_to_id.get(dest, "")
            # Twine often links to a passage whose title equals the choice text.
            if not target_id:
                # try without quotes mismatch
                for t, tid in title_to_id.items():
                    if t.strip('"') == dest.strip('"'):
                        target_id = tid
                        break
            choices.append(
                {
                    "id": f"{node_id}_c{i+1}",
                    "text": label,
                    "nextNodeId": target_id,
                    "setFlags": [],
                }
            )
        nodes.append(
            {
                "id": node_id,
                "speakerId": infer_speaker(line, title),
                "line": line if line else f"(({title}))",
                "choices": choices,
            }
        )

    # Stable order: named beats first then alpha by id.
    beat_order = [
        "prologue",
        "tutorial",
        "entering_calrenoth",
        "calrenoth_drawbridge",
        "combat_sequence_lowering_the_drawbridge",
        "realm_of_darkness_frangitur_segment_1",
        "tutorial_completion",
    ]
    order_index = {n: i for i, n in enumerate(beat_order)}
    nodes.sort(key=lambda n: (order_index.get(n["id"], 999), n["id"]))

    return {
        "schemaVersion": 1,
        "id": "tessera_dialogues",
        "trees": [
            {
                "id": "dlg_act0_wrathful_conquest",
                "parentQuestId": "mq_act0_calrenoth",
                "displayName": "Act 0 — Wrathful Conquest (Twine)",
                "canonStatus": "draft",
                "summary": "Imported from Twine Act 0 Harlowe source. Full Calrenoth → Creotar spine; draft until beat-sheet review.",
                "storyRef": "context/story/sources/wrathful-conquest-act0.twee",
                "entryNodeId": "prologue",
                "nodes": nodes,
                "tags": ["main", "act0", "twine-import"],
                "openQuestions": [
                    "Creotar vs Creo identity",
                    "Crystal location stubs empty in Twine",
                    "Wake-up hub after Tutorial Completion",
                ],
            }
        ],
    }


def main() -> int:
    repo = Path(__file__).resolve().parents[1]
    twee = repo / "context" / "story" / "sources" / "wrathful-conquest-act0.twee"
    out = repo / "samples" / "open-world-rpg" / "assets" / "world-forge" / "dialogues.worldforge.json"
    if not twee.exists():
        print(f"missing {twee}", file=sys.stderr)
        return 1
    data = convert(twee)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    tree = data["trees"][0]
    print(f"wrote {out} — {len(tree['nodes'])} nodes, entry={tree['entryNodeId']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
