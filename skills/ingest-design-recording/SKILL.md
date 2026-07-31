---
name: ingest-design-recording
description: >-
  Ingest Dom/owner voice transcripts and design recordings into Dom Q&A,
  story canon, and World Forge seeds without inventing lookalike entities.
  Use when the user drops a recording transcript, Discord export, design
  session notes, Act 0/1 lore answers, or asks to lock Dom open questions.
---

# Ingest Design Recording

Turn noisy Dom + owner session audio/text into durable canon locks.

**Read first:** [`.cursor/rules/transcript-canon-alignment.mdc`](../../.cursor/rules/transcript-canon-alignment.mdc), [`context/design/dom-open-questions.md`](../../context/design/dom-open-questions.md), [`context/design/dom-answered-questions.md`](../../context/design/dom-answered-questions.md), [`context/story/index.md`](../../context/story/index.md), [`context/decisions/index.md`](../../context/decisions/index.md).

**Not for:** inventing new factions/places from misspellings; silent World Forge forks without an open question or owner confirm.

## Checklist

```
Ingest:
- [ ] Save raw provenance under context/design/recording_*.md (keep misspellings in quotes)
- [ ] Map near-miss spellings → established canon (do not create lookalikes)
- [ ] Move answered Dom rows open → answered; leave true news as open/draft
- [ ] Mirror locks into story docs / World Forge only when IDs already exist or owner locked a new id
- [ ] Scan for accidental alternate spellings after the pass
```

## 1. Provenance file

1. Create `context/design/recording_<slug>_YYYY-MM-DD.md` (or keep the user-supplied name).
2. Header: date, speakers, duration if known, one-paragraph summary.
3. When quoting raw speech, keep the transcript misspelling in quotes and map to canon in the same sentence.
4. Link the provenance from Dom answered rows that came from this session.

## 2. Canon alignment (mandatory)

Prefer existing display names and IDs already in story / World Forge:

| Transcript near-miss | Canon |
| --- | --- |
| Aerotropy / Atrobia / Itro B | **Arrotrebae** |
| Karanoth | **Calrenoth** |
| Fairy Iron / Farium / Nefarian Shroud | **Nefarium Shroud** |
| Sergeant Laurel / Wrench Laurel | **Larrell** / **Grenge** as context demands |
| Arcan | **Arkand** |
| Frangator | **Frangitur** |
| Drool gath / draw gap | **Drul’gath** |

If the name might be **intentionally new**, leave it open/draft and ask the owner — do not silently fork canon.

IDs for new locked entities: snake_case from display title via `engine::slugify_id` ([`ids-from-display-names.mdc`](../../.cursor/rules/ids-from-display-names.mdc)).

## 3. Dom Q&A tables

1. Read `dom-open-questions.md` for matching IDs (D-P0-## / D-P1-## / D-P2-##).
2. For each lock: add a row to `dom-answered-questions.md` (ID, summary, answer, date); remove or mark answered in open.
3. Do **not** re-litigate answered rows without explicit owner reopen.
4. Partial answers stay partial — do not invent the missing half.
5. Update `Last refreshed` on both Dom docs when material.

## 4. Story / World Forge mirrors

Only after Dom lock:

- Story prose under `context/story/` (Act 0 Landfall, companions, factions).
- World Forge JSON via `engine_world_forge_apply` when seeds already exist — do not invent parallel faction ids.
- Decisions: add/update DEC only when the lock changes architecture or product policy.

## 5. Done bar

- Provenance file exists with transcript→canon maps.
- Dom tables updated; no orphan lookalike names in new prose.
- Owner-facing leftovers listed as open IDs, not buried in chat.
