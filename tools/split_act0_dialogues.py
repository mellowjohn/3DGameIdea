#!/usr/bin/env python3
"""Split dlg_act0_wrathful_conquest into per-beat trees. Rewrites dialogues.worldforge.json."""
from __future__ import annotations

import json
from copy import deepcopy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "samples/open-world-rpg/assets/world-forge/dialogues.worldforge.json"

# Cut edges: when walking a beat, do not follow these next ids (they belong to later beats).
SPLITS = [
    {
        "id": "dlg_act0_meet_arkand",
        "displayName": "Act 0 — Meet Arkand (siege approach)",
        "summary": "Frangitur omen + meet Arkand outside Calrenoth. Ends before entering the keep.",
        "entryNodeId": "prologue",
        "stop_at": {"entering_calrenoth", "calrenoth_drawbridge", "realm_of_darkness_frangitur_segment_1", "tutorial_completion"},
        "tags": ["main", "act0", "meet_arkand"],
    },
    {
        "id": "dlg_act0_report_to_grenge",
        "displayName": "Act 0 — Report to Commander Grenge",
        "summary": "Inside Calrenoth: report to Grenge and commit to the drawbridge plan.",
        "entryNodeId": "entering_calrenoth",
        "stop_at": {"calrenoth_drawbridge", "realm_of_darkness_frangitur_segment_1", "tutorial_completion", "prologue", "tutorial"},
        "tags": ["main", "act0", "enter_calrenoth"],
    },
    {
        "id": "dlg_act0_drawbridge_retreat",
        "displayName": "Act 0 — Drawbridge retreat",
        "summary": "Lower the rear drawbridge, Larrell beat, transition into the vision.",
        "entryNodeId": "calrenoth_drawbridge",
        "stop_at": {"realm_of_darkness_frangitur_segment_1", "tutorial_completion", "prologue", "tutorial", "entering_calrenoth"},
        "tags": ["main", "act0", "lower_drawbridge"],
    },
    {
        "id": "dlg_act0_creotar_vision",
        "displayName": "Act 0 — Creotar vision",
        "summary": "Realm of Darkness / Frangitur → Creotar vision and Act 0 close.",
        "entryNodeId": "realm_of_darkness_frangitur_segment_1",
        "stop_at": {"prologue", "tutorial", "entering_calrenoth", "calrenoth_drawbridge"},
        "tags": ["main", "act0", "creotar_vision"],
    },
]


def collect(nodes_by_id: dict, entry: str, stop_at: set[str]) -> set[str]:
    seen: set[str] = set()
    stack = [entry]
    while stack:
        nid = stack.pop()
        if not nid or nid in seen or nid in stop_at:
            continue
        if nid not in nodes_by_id:
            continue
        seen.add(nid)
        for choice in nodes_by_id[nid].get("choices", []):
            nxt = choice.get("nextNodeId") or ""
            if nxt and nxt not in stop_at:
                stack.append(nxt)
    return seen


def trim_tree(nodes_by_id: dict, keep: set[str], stop_at: set[str]) -> list:
    out = []
    for nid in keep:
        node = deepcopy(nodes_by_id[nid])
        for choice in node.get("choices", []):
            nxt = choice.get("nextNodeId") or ""
            if nxt and (nxt in stop_at or nxt not in keep):
                # End this beat; next event/quest stage starts the following tree.
                choice["nextNodeId"] = ""
        out.append(node)
    # Stable-ish order: entry first then alpha
    return out


def main() -> None:
    data = json.loads(PATH.read_text(encoding="utf-8"))
    mega = next(t for t in data["trees"] if t["id"] == "dlg_act0_wrathful_conquest")
    sandbox = next((t for t in data["trees"] if t["id"] == "dlg_sandbox_sample"), None)
    nodes_by_id = {n["id"]: n for n in mega["nodes"]}

    new_trees = []
    for spec in SPLITS:
        keep = collect(nodes_by_id, spec["entryNodeId"], set(spec["stop_at"]))
        if spec["entryNodeId"] not in keep:
            raise SystemExit(f"entry missing from keep: {spec['id']}")
        nodes = trim_tree(nodes_by_id, keep, set(spec["stop_at"]))
        # Put entry first
        nodes.sort(key=lambda n: (0 if n["id"] == spec["entryNodeId"] else 1, n["id"]))
        new_trees.append(
            {
                "id": spec["id"],
                "parentQuestId": "mq_act0_calrenoth",
                "displayName": spec["displayName"],
                "canonStatus": "draft",
                "summary": spec["summary"],
                "storyRef": mega.get("storyRef", ""),
                "entryNodeId": spec["entryNodeId"],
                "nodes": nodes,
                "acts": ["act0"],
                "tags": spec["tags"],
                "openQuestions": [],
            }
        )
        print(f"{spec['id']}: {len(nodes)} nodes, entry={spec['entryNodeId']}")

    # Legacy stub so old refs don't explode; one-line redirect notice.
    new_trees.insert(
        0,
        {
            "id": "dlg_act0_wrathful_conquest",
            "parentQuestId": "mq_act0_calrenoth",
            "displayName": "Act 0 — Legacy mega-tree (deprecated)",
            "canonStatus": "draft",
            "summary": "DEPRECATED: split into dlg_act0_meet_arkand / report_to_grenge / drawbridge_retreat / creotar_vision. Stub kept for old hooks.",
            "storyRef": mega.get("storyRef", ""),
            "entryNodeId": "legacy_stub",
            "nodes": [
                {
                    "id": "legacy_stub",
                    "speakerId": "narrator",
                    "line": "This conversation was split into separate Act 0 event dialogues. Start dlg_act0_meet_arkand for the siege approach.",
                    "choices": [
                        {
                            "id": "legacy_stub_c1",
                            "text": "Understood.",
                            "nextNodeId": "",
                            "setFlags": [],
                            "tone": "Closing",
                        }
                    ],
                }
            ],
            "acts": ["act0"],
            "tags": ["main", "act0", "deprecated"],
            "openQuestions": ["Remove stub once all hooks point at per-beat trees."],
        },
    )

    if sandbox:
        new_trees.append(sandbox)

    data["trees"] = new_trees
    PATH.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print("Wrote", PATH)


if __name__ == "__main__":
    main()
