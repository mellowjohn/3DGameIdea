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
- [ ] Runtime: load/cache PNG via ImGui/D3D12 — **not in this pass** (placeholder draw instead)
- [x] Editor UI tab: image path field + imageMode
- [x] MCP: `engine_ui_canvas_mutate` style/add accepts `image` / `imageMode`
- [ ] Provenance index for real textures — deferred until real assets land
- [x] Sample: main-menu New Game references `assets/ui/textures/btn_new_game.png` (missing → placeholder OK)
- [x] Suite tests: parse `image` field, mutate set image; rebuild `engine`

## Out of scope

Nine-slice borders, SVG, animated GIF, in-engine image generation.

## Dependencies

Blocked by TICKET-0159; needs D3D12/ImGui texture upload path for full acceptance.

## Verification

Placeholder visible when `image` set; suite parse/mutate; `engine_suite_tests --suite hud`.

## Agent notes

**MVP shipped:** schema + editor/MCP + purple placeholder with filename stem. **Follow-on:** Windows WIC (or stb_image) decode + D3D12 SRV / ImGui texture ID cache + hot-reload + provenance for real PNGs under `assets/ui/textures/`.
