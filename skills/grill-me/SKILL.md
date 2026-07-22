---
name: grill-me
description: >-
  Aggressively clarify ambiguous, underspecified, or conflicting requests by
  grilling the user with pointed questions before substantial work. Use when
  the user says "grill me", asks to be questioned on ambiguity, wants
  requirements pressure-tested, or when critical context is missing and
  different answers would materially change the outcome.
---

# Grill Me

Interrogate the user on real ambiguities. Do not implement, plan broadly, or soft-pedal until the blocking choices are pinned down.

## Goal

Force crisp decisions where vagueness would cause rework, wrong architecture, wrong scope, or mismatched success criteria.

## Workflow

1. Restate the topic in one sentence so the user can correct the frame.
2. Inventory ambiguities. Separate:
   - **Discoverable** — inspect the repo/context; do not ask.
   - **Safe default** — state the assumption briefly; do not grill.
   - **Preference** — ask only if taste changes the result.
   - **Blocking** — grill these.
3. Rank blocking ambiguities by blast radius (architecture, scope, compatibility, acceptance, irreversible cost).
4. Ask **one** pointed question at a time.
5. After each answer: restate it as a testable decision, then ask the next blocking question.
6. Stop grilling when remaining unknowns are non-blocking. Summarize locked decisions and open non-blockers, then wait for the user to proceed.

## Question Style

- Be direct and slightly adversarial: challenge fuzzy words ("simple", "later", "like X", "flexible", "polish").
- Prefer concrete options over open prompts. Offer 2–4 real alternatives when useful.
- Recommend a default with a one-line reason when one is defensible.
- Explain **why the answer matters** in one short clause (what breaks if wrong).
- Dig into contradictions with prior answers, repo decisions, or stated goals.
- Reject non-answers: "maybe", "both", "we'll see", "you decide" — push for a choice or an explicit deferral with consequences.
- Do not ask about routine implementation details the project can decide.

## Good vs Bad Questions

```text
❌ BAD: "Any preferences on the UI?"
✅ GOOD: "Is the map editor a full authoring surface or a read-only preview? Full surface means selection, create, and inspectors; preview means pan/zoom only."

❌ BAD: "Should performance be good?"
✅ GOOD: "What's the hard budget: 60 FPS on your machine with the vertical slice loaded, or 'smooth enough' with no metric?"

❌ BAD: "How should IDs work?"
✅ GOOD: "Stable IDs from display names via slugify, or hand-authored opaque codes? Repo convention is slugify."
```

## Guardrails

- Do not dump a questionnaire. One question, then wait.
- Do not pretend certainty is required for every detail; only grill blockers.
- Do not silently resolve blocking ambiguity to keep momentum.
- Do not start implementation during a grill session unless the user explicitly ends grilling and asks to build.
- If the topic is engine architecture/requirements, prefer recording durable outcomes via `interview-engine-decisions` after the grill concludes.

## Exit Summary

When grilling ends, output:

1. **Locked decisions** — bullet list of testable choices
2. **Assumptions** — non-blocking defaults you will use
3. **Still open** — deferred items and why they can wait
