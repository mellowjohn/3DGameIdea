# Dialogue sandbox MCP scenario

Status: active — Act 0 play pipeline smoke for agents and humans

Compact world: [`samples/open-world-rpg/worlds/sandbox.world.json`](../../samples/open-world-rpg/worlds/sandbox.world.json)  
Launch (does not change campaign `defaultWorld`):

```bat
build\windows-msvc-debug\Debug\engine.exe editor --project samples/open-world-rpg --world worlds/sandbox.world.json
```

Helper: [`tools/run-sandbox-editor.cmd`](../../tools/run-sandbox-editor.cmd). From a running campaign editor you can also **File → Open World → sandbox** (session-only; does not change `defaultWorld`).

Keep Cursor MCP pointed at the same project (`engine mcp --project …/samples/open-world-rpg`). Enable **Diagnostics → MCP connection** (or `engine_editor_live` `enable`).

## Sandbox layout (circular pad)

Dry **circular test pad** centered near `(20, 20)`, flat height **4 m** (above sea level `0.35`) with a light berm ring, grass paint, and sparse grass/flower foliage. Oaks/bushes sit on the rim for a small enclosed arena.

| Object | Purpose |
| --- | --- |
| `player` | Play-test spawn on the south edge of the pad |
| `campfire` | Pad center — **Press E to rest** (`use_campfire` heal + audio) |
| `arkand_npc` | Talkable NPC — **sandbox sample** dialogue (`talk_sandbox` → `dlg_sandbox_sample`), not Act 0 story |
| `talk_act0_marker` | Extra volume using the same `talk_sandbox` binding |
| `event_zone` | **Press E to investigate** → sandbox timeline |
| Stones / dead log / stump / crates | Camp props on the flat |
| Oak + bush rim | Visual enclosure |
| `Sun` | Directional light |

Play HUD (screen): HP + class resource (stamina/magic) + quest objective. Prompts are world billboards.

**Talk flow:**

1. Walk into the NPC volume → world billboard **Press E to talk** (dialogue must **not** auto-start on enter)
2. Press **E** → line page opens (`dlg_sandbox_sample` / Practice Keeper) with typewriter body + **Continue**
3. **Continue** while typing → skip typewriter; **Continue** again → **choices page**
4. Click a choice → next line page (or end)

Campaign `talk_act0` volumes use the same Press-E gate (no walk-in Act 0 / Frangitur start). Act 0 mega-tree split is tracked as **TICKET-0224**.

Body text uses Roboto with a right-side scrollbar when lines overflow; speaker stays Cinzel.

Shares World Forge dialogue catalog with the campaign project (sandbox tree is a separate entry). Terrain/paint/foliage stores are **project-global**.

## Scenario A — MCP dialogue walk (no walking required)

After Start Test (F5) optional; dialogue_call works during play test.

1. `engine_editor_status` — confirm live bridge.
2. `engine_dialogue_call` `{ "kind": "reset" }`
3. `engine_dialogue_call` `{ "kind": "start", "treeId": "dlg_sandbox_sample" }`  
   Expect: `treeId`, `nodeId`, `choiceIds`, dialogue canvas on **line page** (body is the line only — choices are not appended as text).
4. `engine_dialogue_call` `{ "kind": "continue" }` — skip typewriter / open **choices page** (`choicesPage=true`).
5. `engine_dialogue_call` `{ "kind": "choose", "choiceId": "<from choiceIds>" }` — applies `standingAdjust` / `setFlags` when authored; returns to line page for the next node.
6. Repeat continue/choose until `complete=true` (or `reset`).
7. `engine_dialogue_call` `{ "kind": "status" }` — inactive when finished/reset.

## Scenario B — Quest hook → dialogue (campaign)

1. `engine_quest_call` `{ "kind": "start", "questId": "mq_act0_calrenoth" }`
2. Optional: Lua via interaction — `engine_lua_call`  
   `{ "kind": "interaction", "id": "talk_act0", "type": "enter" }`  
   Expect blackboard `dialogue.talk_act0.started=true` and an active tree (quest start hook preferred).
3. Re-enter same call — should skip (one-shot guard).
4. `engine_dialogue_call` `{ "kind": "reset" }` then walk choices as in A (use `continue` before `choose`).

## Scenario C — In-world volume (manual)

1. Start Test on Game tab (sandbox world).
2. Walk into Arkand’s gold talk volume → **Press E**.
3. Dialogue canvas: line → Continue → choices with hover + hints → pick.
4. Diagnostics → **Dialogue runtime** shows tree/node.

## Scenario D — Session flags + quest fork (TICKET-0225 / DEC-0046)

Live MCP smoke for `FlagRuntime`. Prefer after Start Test so HUD/quest binds are hot; `flag_call` / `quest_call` / `dialogue_call` are play-test safe.

**D1 — Direct flag API**

1. `engine_flag_call` `{ "kind": "clear", "flagId": "sandbox.peace_kept" }` (ok if missing).
2. `engine_flag_call` `{ "kind": "set", "flagId": "sandbox.probe" }`
3. `engine_flag_call` `{ "kind": "has", "flagId": "sandbox.probe" }` — expect `has=true`.
4. `engine_flag_call` `{ "kind": "list" }` — expect `sandbox.probe` in `flagIds`.
5. `engine_flag_call` `{ "kind": "clear", "flagId": "sandbox.probe" }` then `has` → `false`.

**D2 — Dialogue `setFlags` → FlagRuntime**

1. `engine_dialogue_call` `{ "kind": "reset" }`
2. `engine_dialogue_call` `{ "kind": "start", "treeId": "dlg_sandbox_sample" }`
3. `engine_dialogue_call` `{ "kind": "continue" }` — choices page.
4. `engine_dialogue_call` `{ "kind": "choose", "choiceId": "greeting_c2" }`  
   Authored `setFlags: ["sandbox.peace_kept"]` (Honest / keep-the-peace branch).
5. `engine_flag_call` `{ "kind": "has", "flagId": "sandbox.peace_kept" }` — expect `has=true`.
6. `engine_dialogue_call` `{ "kind": "reset" }` (optional cleanup).

**D3 — Quest `resolve_fork`**

Uses campaign quest seed `mq_act0_calrenoth` / fork `larrell_save_vs_flee` (safe in sandbox MCP; does not require Act 0 world).

1. `engine_quest_call` `{ "kind": "start", "questId": "mq_act0_calrenoth" }`
2. `engine_quest_call` `{ "kind": "resolve_fork", "questId": "mq_act0_calrenoth", "forkId": "larrell_save_vs_flee", "outcomeFlag": "act0.helped_larrell" }`  
   Expect success + `hasFlag=true`.
3. `engine_flag_call` `{ "kind": "has", "flagId": "act0.helped_larrell" }` — `true`.
4. `engine_quest_call` `{ "kind": "resolve_fork", "questId": "mq_act0_calrenoth", "forkId": "larrell_save_vs_flee", "outcomeFlag": "act0.fled_drawbridge" }`  
   Sibling clear: `act0.helped_larrell` false, `act0.fled_drawbridge` true.
5. Optional: `engine_quest_call` `{ "kind": "abandon", "questId": "mq_act0_calrenoth" }`.

## Juice / tween note

Canvas button hover uses a tiny engine-owned `ease_out_cubic` approach (no new dependency). Preferred future UI tween library after license provenance: **ImAnim** (MIT, ImGui-native). Tweeny remains a candidate for non-UI engine tweens.

## Headless regression

`engine_suite_tests --suite automation` covers `dialogue_call` start/present/choose/reset, `talk_act0` enter guard, `flag_call`, and `quest_call` `resolve_fork` (no editor required). `world_forge` covers `FlagRuntime` + fork sibling clear.

## Notes

- Dialogue finish does **not** auto-complete quests ([DEC-0028](../decisions/index.md#dec-0028-explicit-quest-progression-runtime)).
- Campaign world remains `worlds/vertical-slice.world.json` via `project.engine.json` `defaultWorld`.
- Reload Cursor MCP after rebuilding `engine.exe` so `engine_dialogue_call` / `engine_flag_call` are listed (including `continue` / `resolve_fork`).
- Soft-gate / journal UI are **not** part of Scenario D ([DEC-0046](../decisions/index.md#dec-0046-session-story-flag-runtime)).
