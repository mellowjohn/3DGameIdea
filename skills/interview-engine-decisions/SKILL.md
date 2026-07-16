---
name: interview-engine-decisions
description: Interview the project owner to resolve ambiguous requirements and record durable decisions for the 3D game engine. Use before architecture-changing work, when requirements conflict or are missing, when a milestone lacks acceptance criteria, or when the user asks to clarify, plan, scope, or choose an engine feature.
---

# Interview Engine Decisions

Resolve only ambiguities that materially affect architecture, scope, compatibility, or acceptance criteria.

## Workflow

1. Read `/AGENTS.md`, `/context/README.md`, `/context/decisions/index.md`, and context relevant to the requested work.
2. Separate unknowns into discoverable facts, safe defaults, preferences, and blocking decisions. Inspect the repository instead of asking about discoverable facts.
3. Use `references/question-bank.md` to identify the smallest blocking decision. Do not dump the entire questionnaire.
4. Ask one focused question at a time. Explain its impact, offer concrete options, and recommend a default with a short reason.
5. Restate the selected answer as a testable decision. Follow up only when another blocking ambiguity remains.
6. Record accepted decisions in `/context/decisions/index.md`. Record unresolved items in `/context/interviews/open-questions.md` without treating them as decisions.
7. Update affected feature, architecture, resource, and roadmap context in the same change as implementation.

## Guardrails

- Do not ask the user to select routine implementation details that project conventions can decide.
- Do not silently choose a platform, graphics API, language, asset format, scripting model, or distribution target.
- Distinguish a reversible experiment from an architectural commitment.
- Preserve rejected alternatives and rationale when they are likely to be proposed again.
- Stop implementation when a missing answer would cause substantial rework; otherwise state the safe assumption and continue.
- Keep context concise, link to source files, and never copy generated build output into the context library.

## Decision Format

Use the template in `/context/decisions/index.md`. Give each decision a stable ID of the form `DEC-0001` and never reuse IDs.
