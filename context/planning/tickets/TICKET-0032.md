# TICKET-0032: Streaming/LOD/budget acceptance for authored regions

- Epic: EPIC-0004
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ad3efc5695815daaa1f72603936a7c

## Goal

Define acceptance-oriented streaming/LOD/budget expectations so authored World Forge regions (hub spikes, wilderness, chaotic pockets) stay within bounded resident sets on the seamless 4×4 km world — without implementing new stream code or locking FPS numbers yet.

## Context links

- Deliverable: [`../../features/streaming-lod-budgets.md`](../../features/streaming-lod-budgets.md)
- [`../../features/map-design-language.md`](../../features/map-design-language.md) (TICKET-0031)
- [`../../features/open-world-navigation.md`](../../features/open-world-navigation.md) (TICKET-0030, DEC-0032/0033)
- [`../../features/debug-world.md`](../../features/debug-world.md), [`navigation-grid.md`](../../features/navigation-grid.md)
- Numeric gate later: TICKET-0139

## Acceptance criteria

- [x] Doc ties density bands to streaming expectations and resident-set policy.
- [x] Covers FT neighborhood swap, camp/instance handoff, fail-closed nav.
- [x] LOD ladder intent (near/mid/far/map UI) without inventing full mesh LOD pipeline.
- [x] Scenario checklist for later engineering / content review.
- [x] Explicit out of scope: new stream impl, FPS lock (0139), mini-map, Recast.
- [x] `epics.md` + features index link the doc.

## Out of scope

- Implementing stream radius changes, mesh LOD, GPU profilers.
- Final FPS/GPU numeric budgets (TICKET-0139).
- Mini-map UI, Recast, final art layouts.

## Dependencies

- After TICKET-0030/0031 design notes (done / needs-approval).
- Soft-feeds TICKET-0139 and content authoring reviews.

## Verification

- Doc-only review against acceptance checklist and current baselines (40 m terrain stream radius 2, 128 m partition).

## What changed

- Summary: Acceptance doc for how hub/wilderness/chaos density maps onto bounded streaming; FT and camp instance handoff rules; qualitative budget categories; scenario checklist. Defers numeric 1440p/60 to TICKET-0139.
- Files: created `context/features/streaming-lod-budgets.md`; updated features index, epics, nav/map cross-links, this stub.
- Schema / API: none.
- Seed / sample data: none.
- Tests / verification: doc review.
- Decisions & tradeoffs: keep current stream radius 2 as baseline; instances drop overland pressure; no silent neighborhood growth.
- Leftover risk: exact camp hibernate vs teardown and shipping radius still open preferences.

## Agent notes

Picked up after owner approved continuing EPIC-0004 with 0032.
