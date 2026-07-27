# TICKET-0164: UI image assets (textures on widgets)

- Epic: EPIC-0007
- Status: needs-approval
- Agent: cursor-agent
- Priority: P2
- Notion: https://app.notion.com/p/39ed3efc569581debbadda50eca0ba7d

## Goal

Allow UI canvases to reference project image files (including agent-generated art) for buttons, panels, and dedicated image widgets — with runtime draw, editor preview, MCP authoring, and documented license provenance.

## Context links

- [DEC-0025](../../decisions/index.md#dec-0025-responsive-ui-canvas-stack-editor--mcp)
- [`context/features/ui-canvas.md`](../../features/ui-canvas.md)
- [`context/formats/ui-canvas-assets.md`](../../formats/ui-canvas-assets.md)
- `AGENTS.md` — permissive license + provenance for all distributed assets
- Predecessors: TICKET-0159 (button), TICKET-0160 (optional toggle/slider chrome)

## Acceptance criteria

- [x] Widget schema: optional `image` (project-relative path); `imageMode` v1: `stretch` | `contain`
- [x] Widget types: dedicated `image` widget (non-interactive) and `image` on `button` / `panel`
- [x] Runtime: load/cache PNG via `UiTextureCache` + WIC/`load_png_imgui_srv` (ImGui heap SRV 256–511); placeholder fallback when missing
- [x] Editor UI tab: image path field + imageMode
- [x] MCP: `engine_ui_canvas_mutate` style/add accepts `image` / `imageMode`
- [x] Provenance for sample chrome packs (`assets/ui/hud/`, `assets/ui/dialogue/`, `assets/ui/textures/PROVENANCE.md`) — still prototype / owner redraw before ship
- [x] Sample: main-menu New Game → `assets/ui/textures/btn_new_game.png`; HUD/dialogue canvases reference chrome PNGs
- [x] Suite tests: parse `image` field, mutate set image, `hud_image_fit_rect` stretch/contain; rebuild `engine`

## Out of scope

Nine-slice borders, SVG, animated GIF, in-engine image generation.

## Dependencies

Blocked by TICKET-0159; needs D3D12/ImGui texture upload path for full acceptance.

## Verification

Placeholder visible when `image` set; suite parse/mutate; `engine_suite_tests --suite hud`.

## Agent notes

**Shipped:** schema + editor/MCP + `UiTextureCache` GPU draw (`stretch`/`contain`) with purple placeholder fallback. Texture cache is path-keyed (survives canvas hot-reload). Nine-slice / SVG still out of scope.
