---
name: desktop-qa
description: >-
  Run an efficient Windows desktop approval session for needs-approval tickets:
  rebuild engine, batch automated gates, spot-check editor/viewport, approve or
  defer in Notion/epics.md. Use when the owner says "desktop QA", "approval
  session", "clear needs-approval", or is back at a machine after mobile-deferred
  review.
---

# Desktop QA

Owner workflow for clearing the `needs-approval` backlog on **Windows + desktop editor**. Pair with [`evaluate`](../evaluate/SKILL.md) (verdict per ticket) and [`engine-ticket-workflow`](../engine-ticket-workflow/SKILL.md) (status rules).

Agents may **prepare** a session plan but **only the owner** runs desktop steps and moves tickets to `done`.

## When to use

- Owner is at a desktop machine and wants to batch-approve deferred work.
- After cloud-agent delivery landed many `needs-approval` tickets.
- Before starting new agent work that depends on approved foundations (World Forge, UI, rigidbody, etc.).

## Do not use for

- Pre-work scoping (`grill-me`, `interview-engine-decisions`).
- Headless-only verification when automated gates already suffice (`evaluate` → approve from stub).
- Implementing fixes — open a rework ticket or set Status back to `active`.

## Session prep (15 min)

1. **Pull** latest `master` (or the PR branches you intend to merge).
2. **Kill** stale `engine.exe` (Debug build path) if rebuild locks — see reset-editor rule.
3. **Rebuild** `engine` target once for the session baseline:
   ```text
   MSBuild engine.sln /t:engine /p:Configuration=Debug
   ```
4. Open **[Approval kanban](https://app.notion.com/p/8689089ead5242f1936074a4ed1d759b?v=39ad3efc56958148b28e000cafe4dd99)** or filter `epics.md` for `needs-approval`.
5. Optional: run [`evaluate`](../evaluate/SKILL.md) on the pile first to sort **approve** vs **defer-desktop** vs **rework**.

Record blockers immediately in ticket **Agent notes**; do not silently skip.

## Phase 1 — Automated gates (run once)

Run from repo root; sample project path shown.

```powershell
cd build\windows-msvc-debug\Debug
.\engine.exe validate --project ..\..\..\samples\open-world-rpg --json
.\engine.exe test --project ..\..\..\samples\open-world-rpg --suite m5-exit --json
.\engine.exe animation-preview --project ..\..\..\samples\open-world-rpg --json
```

| Gate | Pass means |
| --- | --- |
| `validate` | Project data + assets coherent |
| `m5-exit` | animator, character, interaction, combat, scripting suites green |
| `animation-preview` | Sample controller tick JSON looks sane |

If **m5-exit fails**, stop broad approval — fix or rework failing suite before approving animation/M5 tickets (e.g. 0110, 0105, 0106).

Optional deeper gate (if time):

```powershell
.\engine.exe test --project ..\..\..\samples\open-world-rpg --suite world_forge --json
.\engine.exe test --project ..\..\..\samples\open-world-rpg --suite automation --json
```

Paste suite summaries into a session log (Notion note or `context/testing/findings.md` only if material defect).

## Phase 2 — Launch editor (one session)

```powershell
.\engine.exe editor --project ..\..\..\samples\open-world-rpg --console
```

Keep **one** editor/MCP process for the session. After C++ merges mid-session: kill → rebuild → relaunch.

## Phase 3 — Batch spot-check order

Work in **batches** (same viewport context). For each ticket: read stub **What changed** → minimal check → [`evaluate`](../evaluate/SKILL.md) verdict → drag Notion **needs-approval → done** (mirror `done` in `epics.md`).

### Batch A — Quick wins (doc / headless already proven)

| Tickets | Spot-check |
| --- | --- |
| 0148 | Open linked `components.md`; skim matches inspector reality |
| 0192 | Read `authoring-git-sync.md` checklist |
| 0146 | Trigger a log line; confirm GPU fields in JSONL (optional) |

**Approve if:** doc matches repo; no surprise missing surfaces.

### Batch B — M5 / rendering (Game + Diagnostics)

| Tickets | Spot-check |
| --- | --- |
| 0110 | Phase 1 gates passed; optional Inspector animator fields on player entity |
| 0191 | Player mesh shows albedo atlas in Game viewport |
| 0105, 0106 | Play test + script/event if stub describes; rig JSON loads in inspector |

**Approve if:** Phase 1 green + visual spot-check matches **What changed**.

### Batch C — World Forge (World Forge tab)

| Tickets | One pass covering many |
| --- | --- |
| 0011–0017, 0183–0208 | Open World Forge: Hierarchy, Relationships graph, Map canvas, Quests, Dialogues; Reload/Save; Act lens toggle |

**Approve batch if:** panes open, sample data loads, one edit+save round-trip on a non-critical field succeeds.

**Rework if:** crash on open, save loses data, or stub claims MCP path that fails offline.

### Batch D — Editor components (Scene + Inspector)

| Tickets | Spot-check |
| --- | --- |
| 0147–0151 | Add Component collider/script; green collider overlay; prefab inherit |
| 0182 | Design Docs tab reads a context MD file |

### Batch E — UI canvas (UI tab + Game HUD)

| Tickets | Spot-check |
| --- | --- |
| 0153, 0157–0164, 0209 | Canvas editor drag; pause/inventory/dialogue modals; parchment chrome |

### Batch F — Git sync + rigidbody (Diagnostics + play test)

| Tickets | Spot-check |
| --- | --- |
| 0193–0195 | Project Sync panel: status; dry-run fetch/pull awareness |
| 0196–0199 | **Chain** — approve all or none: player on rigidbody, crate physics, root motion |

### Batch G — Quest/dialogue (World Forge + play)

| Tickets | Spot-check |
| --- | --- |
| 0050–0054, 0165–0168, 0180–0181 | Quests pane; dialogue graph; one Twine tree opens; quest_call smoke if time |

### Defer (do not block session)

- Design-only **0030–0032** — read and approve on prose unless you want map walk validation.
- **P3** or tickets whose stubs still say `proposed` while epics says `needs-approval` → **rework** sync first.

## Per-ticket decision

| Outcome | Action |
| --- | --- |
| **Approve** | Notion → `done`; mirror in `epics.md` (owner only) |
| **Defer** | Leave `needs-approval`; note why in Agent notes |
| **Rework** | Leave `needs-approval` or set `active`; list concrete failure |
| **Reject scope** | Discuss before `deferred`; do not delete agent work unilaterally |

Use [`evaluate`](../evaluate/SKILL.md) output template when unsure.

## Session log (copy/paste)

```markdown
## Desktop QA — YYYY-MM-DD

**Baseline:** commit ______ | engine rebuild: pass/fail
**Gates:** validate __ | m5-exit __ | animation-preview __

| Ticket | Verdict | Notes |
| --- | --- | --- |
| 0110 | done / defer / rework | |

**Follow-ups:** (tickets to re-agent, findings for findings.md)
```

## Time-boxing

| Time | Focus |
| --- | --- |
| 30 min | Phase 1 gates + Batch A + TICKET-0110/0191 |
| 60 min | + Batch C World Forge smoke |
| 90 min | + B + D or E (pick one epic family) |
| 2+ hr | Full pile; take breaks between batches |

Stop when a **batch** fails badly — fix root cause before approving siblings in that batch.

## Agent role during owner QA

When user says "desktop QA session":

1. List current `needs-approval` IDs from `epics.md` grouped by batch above.
2. Emit Phase 1 commands tailored to their build paths.
3. For a named ticket, run [`evaluate`](../evaluate/SKILL.md) and give **one** spot-check step.
4. Do **not** set Status to `done`.
5. After session, offer to update stubs/findings from owner's log.

## Good vs bad session

```text
❌ BAD: Approve 40 World Forge tickets without opening World Forge
❌ BAD: Approve 0198 but not 0197/0199 (rigidbody chain)
✅ GOOD: m5-exit green → approve 0110 → WF tab smoke → approve 0015–0017 cluster
✅ GOOD: 0191 fails albedo → defer 0191, still approve 0148 doc ticket
```

## Additional resources

- [`evaluate/SKILL.md`](../evaluate/SKILL.md)
- [`epics.md`](../../context/planning/epics.md) — Suggested work order, Owner QA note
- [`notion-sync.md`](../../context/planning/notion-sync.md) — Approval kanban
- [`testing/findings.md`](../../context/testing/findings.md) — material defects
- [`features/animator.md`](../../context/features/animator.md) — M5 exit commands
