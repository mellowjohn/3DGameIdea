# TICKET-0243: Stylized flame VFX goal (mesh + particle recipes)

- Epic: EPIC-0005
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/3abd3efc569581c2a018ced28aca2f09

## Goal

Ship an agent-clonable stylized fire look matching the owner references: blob/mesh flame and/or multi-layer particle flame+smoke variants (wispy ribbon, blobby molten, diffuse column), composed from master material profiles + particle recipes + bloom — not a shader graph.

## Context links

- References: `context/art/reference/stylized-flame-blob-mesh.png`, `context/art/reference/stylized-flame-particle-trio.png`
- Feature: `context/features/stylized-flame-vfx.md`
- [DEC-0049](../../decisions/index.md#dec-0049-agent-writable-material-shader-profiles)
- Prerequisites: TICKET-0239–0242

## Acceptance criteria

- [x] At least **one** hero sample prefab agents can place (`stylized_flame`) that reads as stylized fire under Game lighting — not only soft-disc particles.
- [x] Documented approach covers both reference families (mesh emissive stand-in + particle trio variants).
- [x] Recipe/catalog entry so MCP agents can clone without inventing curves from scratch.
- [x] Bloom (0242) landed; flame readable without it, better with it.
- [x] Feature note `context/features/stylized-flame-vfx.md` + features index + resource provenance for any new textures.
- [x] Rebuild `engine` if C++ masters added; validate sample project.

## Out of scope

- Photoreal fire / fluid sim.
- Unity VFX Graph / Niagara parity.
- Full shader-graph authoring UI.
- DoF bokeh matching the mesh reference camera.
- True noise-displace dissolve master (documented follow-on).

## Dependencies

Blocked by / soft after: TICKET-0239, 0240, 0241, 0242. Owner override to implement with bloom.

## Verification

- `engine validate --project samples/open-world-rpg`: valid.
- particles suite after new emitters.
- Place `stylized_flame.prefab.json` in editor for visual likeness vs refs.

## What changed

- Summary: Hero prefab + molten/wispy/column particle recipes + `flame_core_emissive` material; recipe catalog updated; feature docs.
- Files: `stylized_flame.prefab.json`, particle JSONs, `flame_core_emissive.material.json`, `vfx_recipes.json`, stylized-flame feature/goal docs.
- Schema: none new.
- Leftover: dedicated displace/dissolve flame master shader for closer blob-mesh match.

## Agent notes

Shipped with 0242 bloom in same session. Mesh path uses emissive_magic pulse as stand-in for displace master.
