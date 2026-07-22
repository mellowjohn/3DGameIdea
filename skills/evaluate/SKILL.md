---
name: evaluate
description: >-
  Structured post-work evaluation of tickets, PRs, or agent handoffs — score
  acceptance criteria, tag verification type (automated vs owner-desktop), and
  recommend approve / defer / rework. Use when the user says "evaluate",
  "review for approval", "should I approve", batch-triage needs-approval, or
  before an agent sets Status to needs-approval.
---

# Evaluate

Judge whether work meets its ticket acceptance **from evidence in the repo**, not chat history. Produce an owner-ready verdict or an agent pre-handoff gap list.

Pair with [`engine-ticket-workflow`](../engine-ticket-workflow/SKILL.md) (implementation lifecycle) and [`signal-to-noise-optimizer`](../signal-to-noise-optimizer/SKILL.md) (concise output).

## When to use

**Owner**
- Batch or single review of `needs-approval` tickets (especially when desktop QA is deferred).
- "Should I approve TICKET-####?" / "Evaluate the needs-approval pile."
- PR review against linked ticket acceptance.

**Agent**
- Self-check **before** setting Status → `needs-approval`.
- User asks to evaluate a PR or diff against a ticket.

## Do not use for

- Pre-work ambiguity (use [`grill-me`](../grill-me/SKILL.md)).
- Explaining a decision question (use [`teach`](../teach/SKILL.md)).
- Recording a new architecture decision (use [`interview-engine-decisions`](../interview-engine-decisions/SKILL.md)).

## Inputs (read in order)

1. `context/planning/epics.md` row (Status, Priority, Notes).
2. `context/planning/tickets/TICKET-XXXX.md` — Goal, Acceptance, Verification, **What changed**, Agent notes.
3. Linked `context/` docs and decisions cited in the stub.
4. Diff / PR / sample commands output when available.
5. Notion page (optional); repo stub wins on conflicts.

If **What changed** is missing or thin on a `needs-approval` ticket, verdict starts at **rework** until filled ([`ticket-template.md`](../../context/planning/ticket-template.md)).

## Verification classes

Tag every acceptance criterion:

| Class | Meaning | Owner action |
| --- | --- | --- |
| `automated` | Named suite, CLI, validate, or deterministic JSON output | Can approve from evidence alone |
| `owner-desktop` | Editor viewport, play test, GPU, interactive UI | Defer approval until desktop session |
| `doc-review` | Context/decision docs only | Approve from stub + diff |
| `unverified` | Claimed but no evidence in stub/diff | Rework or re-run verification |

**Rule:** `automated` pass does **not** imply `owner-desktop` pass. Split verdicts when mixed.

## Workflow

### A — Owner ticket evaluation

1. Restate Goal in one sentence.
2. List acceptance criteria from the stub; copy checkboxes as written.
3. For each criterion: class → evidence (file path, suite `N/N`, CLI sample, or **missing**) → **met / partial / not met**.
4. Summarize **What changed** in one short paragraph (or flag if absent).
5. Note leftover risk from stub; add any new risk from your read.
6. **Recommendation:** `approve` | `defer-desktop` | `rework` | `reject-scope` — one primary, brief why.
7. If batch triaging: group by recommendation; list ticket IDs only (no full re-evaluation per ticket unless asked).

### B — Agent pre-handoff check

Run before Status → `needs-approval`:

```
Pre-handoff:
- [ ] Every acceptance criterion has a verification class
- [ ] What changed filled (summary, files, schema deltas, samples, test evidence, risks)
- [ ] C++ changed → rebuild stated (pass/fail/not run + why)
- [ ] Behavior change → named suite or CLI evidence cited
- [ ] owner-desktop criteria explicitly listed as "pending owner QA"
- [ ] No criterion marked met without evidence
- [ ] Status stays needs-approval (never done)
```

If any `automated` criterion is not met, **do not** set `needs-approval` — finish verification or leave `active` with blocker noted.

### C — PR evaluation

1. Identify linked `TICKET-####` (PR body or branch context).
2. Map diff hunks to acceptance criteria.
3. Same per-criterion table as (A).
4. Call out scope creep vs ticket Out of scope.

## Output format (default)

Use this structure; omit empty sections.

```markdown
## Evaluation: TICKET-XXXX — <title>

**Recommendation:** approve | defer-desktop | rework | reject-scope
**Confidence:** high | medium | low (low = missing What changed or no test evidence)

### Acceptance
| Criterion | Class | Evidence | Verdict |
| --- | --- | --- | --- |
| … | automated | animator suite cited in stub | met |

### Blockers (if rework/reject)
- …

### Defer to desktop (if defer-desktop)
- …

### Leftover risk
- …

### Suggested owner action
One line: e.g. "Approve automated slice; desktop-verify Inspector overlay in next session."
```

For **batch** triage, compress to a table of ID → recommendation → one-line reason.

## Recommendation rules

| Situation | Recommendation |
| --- | --- |
| All criteria met with evidence; only `doc-review` / `automated` | **approve** |
| All automated met; any `owner-desktop` unchecked | **defer-desktop** (or **approve** partial slice if ticket explicitly split — say so) |
| Missing What changed, missing suites, or stub/epics mismatch | **rework** |
| Diff clearly exceeds Out of scope or wrong milestone | **reject-scope** (ask owner; do not revert unilaterally) |

Agents **never** set Status to `done`. Evaluation may recommend approve; only the owner moves to `done`.

## Good vs bad evaluation

```text
❌ BAD: "Looks good, approve." (no criterion table, no evidence)
❌ BAD: Marking editor UI criteria met because C++ compiled
✅ GOOD: "CLI m5-exit met (automated); Map Canvas drag untested (owner-desktop) → defer-desktop"
✅ GOOD: "0201–0204 epics says needs-approval but stub is proposed → rework sync first"
```

## Additional resources

- [`ticket-template.md`](../../context/planning/ticket-template.md)
- [`epics.md`](../../context/planning/epics.md)
- [`testing/strategy.md`](../../context/testing/strategy.md)
- [`engine-ticket-workflow`](../engine-ticket-workflow/SKILL.md)
