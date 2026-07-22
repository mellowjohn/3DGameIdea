---
name: teach
description: >-
  Explain a confusing question, term, or decision the agent asked so the user
  can answer it informed. Use when the user says "teach", "explain that
  question", "I don't understand", "what do you mean", or asks for help
  understanding options, tradeoffs, or jargon before answering.
---

# Teach

Help the user understand a question (usually one just asked) well enough to choose an answer. Teach, then return them to answering — do not take over the decision.

## Goal

Turn confusion into a clear choice. The user should leave able to pick an option, defer consciously, or ask a sharper follow-up.

## When to Use

- User is stuck on a grill-me / interview / clarify question
- User asks what a term, option, or tradeoff means
- User understands the words but not why the choice matters

## Workflow

1. Identify the exact question or concept they are stuck on (quote it if needed).
2. Explain in plain language:
   - **What is being asked**
   - **Why it matters** (what changes in the product/engine if they pick wrong)
   - **Options** with concrete consequences for each
   - **What it does *not* ask** (disambiguate near-miss interpretations)
3. Give a small example grounded in this repo or the current task when possible.
4. Recommend a default only if one is defensible; label it as a recommendation, not a decision.
5. Re-ask the original question in simpler form, with the same options.
6. Stop teaching and wait for their answer. Do not proceed with implementation unless they answer and ask to continue.

## Teaching Style

- Short, concrete, no lecture padding.
- Prefer analogies only when they reduce jargon; skip cute metaphors that add fog.
- Define terms the first time they appear.
- Contrast options with outcomes, not abstract virtues ("flexible" vs "simple").
- If they are confused because the question was poorly framed, fix the framing and own that — then re-ask cleanly.
- If multiple questions piled up, teach only the current blocker.

## Good vs Bad Teaching

```text
❌ BAD: "It's about architecture. Think carefully about scalability and maintainability."
✅ GOOD: "I'm asking whether NPCs load with the whole world or stream in by region. Whole-world is easier to author now but will hitch on large maps. Streaming costs more engine work up front."

❌ BAD: Dump five related design essays, then ask something new.
✅ GOOD: Explain the one stuck question, show 2–3 options with consequences, re-ask that same question.
```

## Pairing with Grill Me

- If a grill session is active, teach **in place**, then resume grilling with the simplified question.
- Do not skip the decision after teaching; the user still answers.
- If teaching reveals the question was non-blocking, say so and move to the next real blocker.

## Exit Check

Before waiting, the user should be able to answer:

> Given options A/B/(C), which outcome do you want, and what are you willing to give up?
