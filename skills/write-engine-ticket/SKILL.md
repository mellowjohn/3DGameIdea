---
name: write-engine-ticket
description: >-
  Author or upgrade engine backlog tickets with testable acceptance criteria,
  verification steps, and clear scope before Status → ready. Use when creating
  a new TICKET-####, promoting proposed → ready, splitting an epic into children,
  or when the user asks to write, refine, or improve ticket acceptance criteria.
---

# Write Engine Ticket

Author tickets agents can execute **without chat history**. Shape comes from [`ticket-template.md`](../../context/planning/ticket-template.md); this skill teaches **quality** — especially acceptance criteria and verification.

**Not for:** implementing tickets (`skills/engine-ticket-workflow`), architecture forks (`skills/interview-engine-decisions`), or post-ship review (`evaluate` / `desktop-qa` when available).

## When to use

- New ticket or epic child before it enters `epics.md` / Notion
- Promoting `proposed` → `ready` (replace generic stub acceptance)
- Owner says “write a ticket for …”, “split this epic”, “make acceptance testable”
- Before assigning **Agent = cursor-agent** on a card

## Workflow

```
Ticket authoring:
- [ ] Read epic row + roadmap milestone; confirm ticket belongs in epics.md first
- [ ] Write Goal (1–2 sentences, owner-visible outcome — not implementation steps)
- [ ] Link context/ paths + DEC-* ids the implementer must read
- [ ] Draft acceptance criteria (testable, fail-closed where relevant)
- [ ] Write Out of scope (explicit non-goals — primary scope-creep guard)
- [ ] Name Dependencies (blocks / blocked-by / parallel OK)
- [ ] Write Verification (exact commands: rebuild, suite names, validate, desktop vs headless)
- [ ] Route work: C++ vs MCP vs docs-only (content-vs-engine-workflows)
- [ ] Set Priority from epics.md ladder; mirror row + full stub + Notion body
- [ ] Ready gate (below) before Status → ready
```

### 1. Goal

State what **done** means for the owner, not how to build it.

```text
❌ BAD: Add AudioEngine class, link miniaudio, wire render loop
✅ GOOD: Integrate miniaudio for spatial/event playback so M5’s audio roadmap item is met with a permissively licensed, testable foundation
```

Agents finish at **`needs-approval`**; only the human sets **`done`**. Do not write acceptance as “ticket is done.”

### 2. Acceptance criteria — make every line testable

Each checkbox must be verifiable by **command, suite, file path, API name, or error code** — not vibes.

| Pattern | Example criterion |
| --- | --- |
| Named CTest suite | Named `audio` suite covers init, load, play, fail-closed on missing file |
| CLI command | `engine animation-preview --project … --json` returns deterministic report |
| Sample / asset path | Sample project plays `assets/audio/campfire_crackle.wav` on interaction enter |
| Schema / format | `*.rig.json` validates joint list + bone roles; invalid file returns `RIG-JOINT-MISSING` |
| License / provenance | miniaudio via vcpkg; license recorded in `context/resources/index.md` |
| Docs index | Feature note in `context/features/` + row in `context/features/index.md` |
| Fail-closed behavior | Missing audio file returns stable error code; path escape rejected |
| Desktop-only (label it) | Editor play-test: orbit camera collision shortens against static mesh |

**Split vague bullets.** “Audio works” → init/shutdown, one-shot, loop, master volume, spatial listener, sample trigger, suite.

**Prefer headless evidence** when the engine supports it (suites, validate, JSON CLI). Mark **desktop QA required** explicitly when GPU, editor UX, or live device is the only proof.

```text
❌ BAD:
- [ ] Audio integration works well
- [ ] Update docs

✅ GOOD (TICKET-0107 shape):
- [ ] miniaudio via vcpkg; license in context/resources/index.md
- [ ] Engine backend: init/shutdown, one-shot + loop, master volume
- [ ] Spatial listener follows active camera; 3D source API
- [ ] Sample: sound from interaction or animation event path
- [ ] Named `audio` suite: init, load, play, fail-closed on missing file
- [ ] Context docs: feature note + resource provenance
```

### 3. Out of scope

List what agents must **not** build. Pull from later milestones, adjacent epics, and “nice to have” ideas the title suggests.

```text
❌ BAD: (empty)

✅ GOOD:
- Full FMOD-style event graphs, occlusion, reverb zones
- Editor audio browser (M10)
- Combat hit SFX polish (M9)
```

### 4. Verification

Tell the implementer exactly how to prove completion **before** `needs-approval`:

- Rebuild target (`engine`, test target)
- Suite names: `engine test --project samples/open-world-rpg --suite <name>`
- `engine validate --project …` when content/schemas change
- Desktop steps when headless is insufficient
- **What changed** must be filled on repo stub + Notion (see ticket-template) — not optional at handoff

### 5. Route: engine code vs content vs docs

Read [`content-vs-engine-workflows.md`](../../context/architecture/content-vs-engine-workflows.md).

| If the ticket needs… | Route in ticket |
| --- | --- |
| New runtime capability, editor tool, shader, CMake dep | C++ `src/` / `include/`; rebuild `engine`; add/extend suite |
| Scene, prefab, Lua, material, terrain already supported by MCP | MCP commands + `engine_project_validate`; say “no C++ unless gap found” |
| Design / research only | Doc paths; verification = review + index update; no rebuild |

If both: sequence them (e.g. “C++ API first; sample Lua trigger in same ticket”).

### 6. Priority and dependencies

- Priority from [`epics.md`](../../context/planning/epics.md#priority-ladder) — do not mark P0 without owner intent
- Dependencies: **blocks**, **blocked by**, **parallel OK** — name ticket IDs
- Split oversized work; one ticket ≈ one verifiable slice an agent can land in one pass

### 7. Sync (DEC-0015)

1. Add/update row in **`epics.md` first** (ID, title, status, priority, notes)
2. Mirror Notion card (properties + **full body** from template — no empty pages)
3. Add/update **`context/planning/tickets/TICKET-XXXX.md`** when `ready` or `active`

Population rule: [`.cursor/rules/epic-ticket-population.mdc`](../../.cursor/rules/epic-ticket-population.mdc).

## Ready gate — do not set `ready` until

- [ ] Goal is outcome-shaped (not a task list)
- [ ] Every acceptance line is testable; no generic “matches title” stubs
- [ ] Out of scope has at least two concrete non-goals for non-trivial work
- [ ] Verification names commands/suites or explicit desktop QA
- [ ] Context links point to real `context/` paths (and DEC-* if applicable)
- [ ] `epics.md` row + Notion body + repo stub agree on scope
- [ ] Blocking architecture choices resolved or delegated to `interview-engine-decisions`

Generic stubs like “Deliverable matches the ticket title” are **placeholders for `proposed` only** — rewrite before `ready`.

## Anti-patterns (from this repo)

| Anti-pattern | Fix |
| --- | --- |
| `epics.md` row with empty Notes | One-line scope + link to stub |
| Notion properties only, empty body | Full template on page |
| Acceptance copied from stub generator | Replace with ticket-specific testable lines |
| Single criterion “implement X” | Decompose to API, sample, suite, docs |
| Missing fail-closed / error cases | Add “invalid input returns `<CODE>`” where applicable |
| Desktop-only work with no label | Add Verification note: “requires editor/GPU session” |

## Good reference tickets

- [`TICKET-0107.md`](../../context/planning/tickets/TICKET-0107.md) — vcpkg + backend + spatial + sample + suite + docs
- [`TICKET-0105.md`](../../context/planning/tickets/TICKET-0105.md) — event hook + Lua contract + suite coverage
- Contrast (needs rewrite before ready): [`TICKET-0110.md`](../../context/planning/tickets/TICKET-0110.md) — generic stub acceptance

## Handoff to other skills

| Situation | Skill |
| --- | --- |
| Ticket is ready; start implementation | `engine-ticket-workflow` |
| Architecture choice blocks acceptance | `interview-engine-decisions` |
| Owner wants ambiguity pressure-test on draft | `grill-me` |
| Implementation finished | Fill **What changed**; `engine-ticket-workflow` → `needs-approval` |

## Additional resources

- [`ticket-template.md`](../../context/planning/ticket-template.md)
- [`epic-template.md`](../../context/planning/epic-template.md)
- [`notion-sync.md`](../../context/planning/notion-sync.md)
- [`epics.md`](../../context/planning/epics.md)
