# TICKET-0201: Blended water material and render pass

- Epic: EPIC-0016
- Status: proposed
- Agent: unassigned
- Priority: P2
- Notion: (mirror when pulled)

## Goal

Ship a low-poly stylized water render path with blended opacity, reflection, refraction, and deterministic scripted wave vertex displacement so authored water surfaces read as liquid ([DEC-0039](../decisions/index.md#DEC-0039-water-swim-and-hydrology-authoring)).

## Context links

- [`water-hydrology.md`](../../features/water-hydrology.md)
- [`materials.md`](../../formats/materials.md) ΓÇö current masked/blended fail closed
- [DEC-0006](../../decisions/index.md#dec-0006-smooth-low-poly-art-direction)
- EPIC-0005

## Acceptance criteria

- [ ] `water.material.json` sample renders with blended pass (not fail-closed)
- [ ] Reflection + refraction visible in editor viewport at stylized quality
- [ ] Wave motion is deterministic and tunable (time + constants)
- [ ] Matches low-poly facet aesthetic (no high-frequency noise soup)
- [ ] `assets` or rendering suite covers shader constants / regression hook

## Out of scope

Full ocean FFT sim; underwater post-process; lava/magic pools.

## Dependencies

Blocks TICKET-0202, TICKET-0200. Soft: EPIC-0005 material work.

## Verification

Rebuild `engine`; editor viewport water plane test; named test suite pass.

## Agent notes

Wave technique (Gerstner vs summed sines) is engine choice per DEC-0039.

