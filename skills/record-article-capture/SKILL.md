---
name: record-article-capture
description: >-
  Capture Windows desktop video and stills for Wrathful Conquest blog/article
  writing via windows-computer-use MCP plus live editor MCP framing
  (engine_editor_session / engine_editor_camera). Use when recording the editor
  or game for a post, gathering blog images, capturing motion montages, or when
  the user asks for article/screencast footage.
---

# Record Article Capture

Windows-native capture for **blog / article media**, with engine MCP for Scene framing.

- Desktop record/screenshot: `windows-computer-use` ([sshh12/windows-computer-use-mcp](https://github.com/sshh12/windows-computer-use-mcp), MIT). Setup: `tools/windows-computer-use/README.md`.
- Scene framing / play-test: live editor MCP (`engine_editor_session`, `engine_editor_camera`, `engine_editor_screenshot`). Docs: `context/features/mcp-live-editor.md`.

Do **not** use Screencast MCP (`@tmhs/screencast-mcp`): CC-BY-NC-ND, incompatible with this repo’s resource policy.

## When to use

- Gathering screenshots or short clips for `blog/content/posts/`
- Showing motion (particles, camera, UI transitions) that a single still misses
- Owner asks to “record for the article,” “grab blog footage,” or “capture the editor for the post”

## When not to use

- Live scene edits → `skills/live-editor-mcp/SKILL.md`
- Voice/design transcript ingest → `skills/ingest-design-recording/SKILL.md`
- Drafting post prose → `.cursor/rules/blog-writing-voice.mdc` (+ personal `no-ai-slop-blog` if loaded)

## Prep

1. Confirm MCP `windows-computer-use` and `ai-rpg-engine` are connected. After rebuilding `engine`, **reload the ai-rpg-engine MCP server** so new tools appear.
2. Put the subject on screen: editor or play window **visible, not minimized**.
3. **Always maximize before any capture** (see `.cursor/rules/fullscreen-before-capture.mdc`). Prefer Win32 `ShowWindow(hwnd, SW_MAXIMIZE=3)` — never `win+up` on Windows 11 (Snap Assist). Exclusive fullscreen can yield black frames; fall back to maximized borderless.
4. Clear secrets from the capture rectangle.
5. Raw dumps go under `out/article-captures/` (`MCP_OUTPUT_DIR`, gitignored). Do not commit them.

## Scene stills / VFX framing (preferred)

Do **not** fight desktop RMB fly for Scene framing. Use live editor MCP:

```text
1. engine_editor_session  kind=end
                          showEventZones=false
                          showCollisionDebug=false
                          showWorldForgeMapMarkers=false
2. engine_editor_camera   action=deselect          # hide transform/selection gizmos
   (or editor_input Escape / focus with select=false)
3. engine_editor_camera   action=focus_entity
                          name=<unique entity name>
                          distance=4–6
                          height=1.5–2.5
                          yawOffsetDegrees=<orbit>  # 0=south of target; ~90=west looking east
                          select=false              # keep gizmos off after focus
4. engine_editor_input    action=clear             # hide yellow MCP cursor overlay
5. Wait ~0.5–1.2s for particles to settle/read
6. engine_editor_screenshot  filename=<stem>  clientAreaOnly=true
7. Review the PNG. Re-shoot if a prop/tree/player blocks the subject or gizmos remain.
```

### Framing checklist (fail closed)

| Check | Fail looks like | Fix |
| --- | --- | --- |
| Play-test stopped | Game HUD / “Test session: …” | `engine_editor_session` `kind=end` |
| No selection gizmos | Yellow AABB, translate arrows, capsule wire | `deselect` / `select=false` / Escape |
| Overlays off | Purple event rings, collision debug | `set_overlays` bools false |
| Subject fills frame | Bird’s-eye clearing, tiny fire | Lower `distance`/`height`; use `yawOffsetDegrees` |
| Clear sightline | Tree / player / rock in foreground | Orbit with `yawOffsetDegrees` or `set_pose` from open side |
| MCP cursor off | Yellow circle in viewport | `engine_editor_input` `action=clear` |
| Motion visible | Flat/static fire in review | Wait a beat; confirm emitters warm; re-record |

`focus_entity` default pose sits **south of the target**. For campfires/props surrounded by trees, pick an open azimuth (`yawOffsetDegrees`) or `set_pose` with explicit `x/y/z` + `yawDegrees`/`pitchDegrees`.

## Game / play-test motion

For gameplay camera motion use `engine_coop_call` / play-test, not Scene DebugCamera. Still maximize before `record`.

## Capture workflow (desktop)

Prefer **observe-only** desktop tools. Do not `act` / `play` unless the owner wants a scripted take.

### Always before still or motion

1. `window` `list` / `focus` on the target (e.g. `AI RPG Engine Editor`).
2. Maximize via Win32 (not `win+up`).
3. Brief wait (~0.5s).
4. Confirm fill with a quick `screenshot` if unsure.

### Still

1. Prefer `engine_editor_screenshot` after MCP framing (GPU backbuffer, project `out/`).
2. Or desktop `screenshot` `target=window:<title>`.
3. Review the image before publishing. Re-shoot on framing failures above.

### Motion (short)

1. Frame first (Scene MCP or play-test).
2. Maximize + foreground the window.
3. `record` with `target=window:AI RPG Engine Editor` (or Game title), `seconds` 5–8, `fps` 10–12, `montage_frames` 6–8.
4. **Read the montage.** Reject if frames are identical, black, or show nothing useful — then re-maximize / re-frame / retry.
5. Encode for the blog with ffmpeg before copying into `blog/public/videos/`:

```text
ffmpeg -y -i out/article-captures/record_....mp4 -vf "scale=1280:-2" ^
  -c:v libx264 -pix_fmt yuv420p -profile:v high -crf 23 -movflags +faststart -an ^
  blog/public/videos/<slug>.mp4
```

6. Keep the raw `mp4` only under `out/article-captures/`.

### Optional driven take

Only if the owner wants a scripted walkthrough: `play` while recording. Prefer human-driven takes for public honesty when motion is the point. Desktop RMB hold is unreliable for Scene fly — use `engine_editor_camera` instead.

## Publish into the blog

1. Pick 1–3 stills that prove the claim (not decorative filler). Prefer clean Scene framing over wide Game HUD when the claim is a prop/VFX.
2. Copy into `blog/public/images/` with a stable name (`blog-<topic>-<beat>.jpg` / `.webp`). Match existing posts.
3. Reference as `/images/<file>` and `/videos/<file>`.
4. Captions: one concrete sentence. No ticket IDs, env vars, or Notion links in visitor copy.
5. Writing voice: `.cursor/rules/blog-writing-voice.mdc`.
6. Leave `draft: true` until the owner asks to publish.
7. Inline `<video>` tags need `width`/`height` (source pixels) plus `preload="metadata"`. Site CSS (`.prose video`) uses `aspect-ratio: 16 / 9` so the slot is not squished before play.

## Safety

- Capture is explicit tool calls only; anything in the target rectangle is recorded.
- Prefer `window:` over full `desktop` so Cursor chat stays out.
- Redact or re-record before copying into `blog/public/`.
- Do not commit `out/article-captures/` or the tools `.venv`.

## Troubleshooting

| Symptom | Fix |
| --- | --- |
| MCP missing / import error | Bootstrap `tools/windows-computer-use`; check `.cursor/mcp.json` |
| New editor tools missing after rebuild | Reload **ai-rpg-engine** MCP server in Cursor |
| `No module named 'mcp.server.fastmcp'` | `.\.venv\Scripts\python.exe -m pip install -r requirements.txt` (`mcp<2`) |
| `mp4: FAILED` / ffmpeg missing | `winget install Gyan.FFmpeg`, restart shell / Cursor |
| Black / identical / empty video | Maximize + foreground; confirm Scene has visible subject; reject montage and retry; re-encode with yuv420p |
| Gizmos in stills | `deselect` / `select=false` / Escape; `editor_input clear` |
| Tree / player blocks subject | `yawOffsetDegrees` or `set_pose` from open side |
| Wrong monitor | `system` displays → `display:N` or tighter `window:` title |

## Additional resources

- Live editor MCP: `context/features/mcp-live-editor.md`
- Blog authoring: `blog/README.md`
- Voice rules: `.cursor/rules/blog-writing-voice.mdc`
- Related ticket: `context/planning/tickets/TICKET-0244.md`
