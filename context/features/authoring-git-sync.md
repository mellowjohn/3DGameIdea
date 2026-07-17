# Authoring Git Sync (Project Sync)

Status: planned (workflow docs ready; in-editor sync EPIC-0014)  
Decision: [DEC-0037](../decisions/index.md#dec-0037-git-backed-authoring-sync-in-editor)  
Epic: [EPIC-0014](../planning/epics.md) (TICKET-0192–0195)

## Purpose

Let multiple humans (and agents) share **project authoring** — especially World Forge story data — by syncing the same git remote, eventually from inside the editor, without a custom cloud-save service.

Example loop:

1. Author A edits World Forge → **Save** (writes `*.worldforge.json`).
2. Author A **commits + pushes** those files (today via git CLI/IDE; soon via in-editor **Project Sync**).
3. Author B **pulls**, **Reloads** World Forge → sees the new story content.

## What syncs

Anything under the project that is tracked by git and already designed to be diffable:

- World Forge assets (`assets/world-forge/*.worldforge.json`) — pantheon, factions, archetypes, relationships, map, quests, dialogues, resources
- Prefabs, materials, scenes, Lua, UI canvases
- Story/context markdown when it lives in the same repo (`context/story/`, etc.)

Player runtime save-games are **not** this feature. Live multi-user co-editing of one open session is **not** this feature.

## Checklist (use today)

### Before you edit shared content

1. Open the same git project/repo as your collaborators (e.g. `samples/open-world-rpg` inside this engine repo, or the agreed content root).
2. `git fetch` then `git pull` (or your IDE equivalent) so you start from remote HEAD.
3. Launch the editor on that project.

### After World Forge / content edits

1. In World Forge, click **Save** so disk JSON matches what you authored.
2. Outside the editor (until TICKET-0194 lands): review `git status` — expect paths under `assets/world-forge/` and any other intentional content.
3. Commit with a short message (e.g. `Add Act 0 flower-quest dialogue beats`).
4. `git push` to the shared remote.

### On the other machine

1. `git pull`.
2. If the editor is already open: World Forge **Reload**. If Scene/Sculpt has unsaved work that overlaps pulled files, save or discard first — the live editor owns in-memory scene authority ([content-vs-engine-workflows.md](../architecture/content-vs-engine-workflows.md)).
3. Confirm the new quests/dialogues/factions appear in World Forge.

### On conflict

1. Do not keep clicking Save over conflict markers.
2. Resolve with git / your usual merge tools; re-open or Reload after a clean working tree.
3. v1 will list conflicted paths in Project Sync but will **not** ship a custom three-way merge UI (TICKET-0193/0194).

## Editor workflow (target — TICKET-0194/0195)

| Step | Action |
| --- | --- |
| Before editing shared files | **Fetch** / **Pull** so you start from remote HEAD |
| After World Forge / content Save | Review **Status**; **Commit** with a short message; **Push** |
| After someone else pushed | **Pull**; accept **Reload** for World Forge (save or discard dirty Scene/Sculpt first if prompted) |
| On conflict | Editor lists conflicted paths; resolve with git/external tools |

## Engine contract (target — TICKET-0193)

- Command-backed ops (GUI + headless/MCP): `status`, `fetch`, `pull`, `commit`, `push`.
- Invoke system `git` for the opened project root; use OS credential helper / SSH agent.
- Structured errors when git is missing, the folder is not a repo, auth fails, or pull conflicts.
- No remotes passwords or PATs stored in project JSON or engine config.
- `commit` requires an explicit message; never auto-commit on every Save.

## Out of scope (v1)

- Live simultaneous editing of one open editor session
- Hosted proprietary sync backend
- Player cloud saves / multiplayer
- Full merge-conflict resolution UI inside the engine
- Auto-commit on every Save

## Related

- [DEC-0003](../decisions/index.md#dec-0003-automation-first-tools) — automation-first tools
- [content-vs-engine-workflows.md](../architecture/content-vs-engine-workflows.md) — World Forge vs scene ownership
- [world-forge-scope.md](world-forge-scope.md) — narrative assets that sync via these files
- Tickets: [`TICKET-0192.md`](../planning/tickets/TICKET-0192.md) · [`TICKET-0193.md`](../planning/tickets/TICKET-0193.md) · [`TICKET-0194.md`](../planning/tickets/TICKET-0194.md) · [`TICKET-0195.md`](../planning/tickets/TICKET-0195.md)
