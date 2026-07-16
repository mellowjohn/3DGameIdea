# TICKET-0144: Typography/font roles + fallback + licenses

- Epic: EPIC-0013
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ad3efc5695818a910fed0456987649

## Goal

Lock font roles: **Roboto** for engine/editor chrome (transparent tooling), **Cinzel** for all in-scene game UI assets, **JetBrains Mono** for diagnostics — with OFL packaging and ImGui load + fallback.

## Context links

- [`context/art/visual-direction.md`](../../art/visual-direction.md)
- [`context/resources/index.md`](../../resources/index.md)
- [`assets/ui/fonts/README.md`](../../../assets/ui/fonts/README.md)

## Acceptance criteria

- [x] Roboto + Cinzel + JetBrains Mono vendored under `assets/ui/fonts/` with `OFL.txt` + PROVENANCE
- [x] Resource registry + visual-direction: Roboto = engine, Cinzel = scene/game
- [x] `GameFonts::load`: Roboto ImGui default; Cinzel for canvas/HUD; JetBrains Mono; FA merge
- [x] Missing fonts fall back with warnings
- [x] Rebuild `engine`

## Out of scope

Full localization shaping; variable-font weight axes in ImGui.

## Verification

Launch editor — Hierarchy/Inspector read as Roboto; Game viewport pause/main menu overlays use Cinzel.

## Agent notes

2026-07-15: Owner chose Cinzel for in-scene game assets; Roboto for engine transparency (supersedes Source Sans 3 for engine UI). Source Sans 3 left archived on disk.
