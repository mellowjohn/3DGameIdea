# RPG Save Game (`*.save.json`)

Status: active (schemaVersion 1) — TICKET-0114 · [DEC-0042](../decisions/index.md#dec-0042-online-co-op-session-model-mode-locked-saves)

Versioned player save format. Distinct from **project authoring** (git-tracked assets) and **editor session** state.

## Default path

`<user-save-root>/<slot-id>.save.json`

Exact save root defaults to a caller-chosen path (e.g. AppData or temp). Engine API: `RpgSaveDocument::load` / `save` / `hydrate_into` / `apply_rpg_save_to_session` (`include/engine/save/rpg_save.h`). Saves must **not** live inside the git-tracked project tree by default.

## Design rules ([DEC-0042](../decisions/index.md#dec-0042-online-co-op-session-model-mode-locked-saves))

1. Root **`sessionMode`**: `"solo"` | `"coop"` — immutable for the life of the save.
2. **`guestProfile`** required when `sessionMode == "coop"`; must be absent or null in solo saves.
3. **`sharedCampaign`** holds all progression both players share (quests, standing, morality, flags, discovery, party, camp, world anchors).
4. Loading co-op save without a connected guest → **fail closed** (lobby only, no `playing`).
5. Solo save → reject co-op lobby join on that slot.

## Top-level shape

```json
{
  "schemaVersion": 1,
  "saveId": "550e8400-e29b-41d4-a716-446655440000",
  "displayName": "Evergreen Wake",
  "sessionMode": "solo",
  "createdAt": "2026-07-22T14:30:00Z",
  "updatedAt": "2026-07-22T18:05:00Z",
  "playTimeSeconds": 14400,
  "difficulty": "normal",
  "worldAnchor": {
    "sceneId": "tessera_overland",
    "regionId": "ohund_evergreens",
    "position": [120.5, 14.2, -880.0],
    "yawDegrees": 192.0
  },
  "hostProfile": { },
  "guestProfile": null,
  "sharedCampaign": { }
}
```

### Co-op example (truncated)

```json
{
  "schemaVersion": 1,
  "saveId": "7c9e6679-7425-40de-944b-e07fc1f90ae7",
  "displayName": "Ledgeport Run (Co-op)",
  "sessionMode": "coop",
  "difficulty": "normal",
  "hostProfile": {
    "playerSlot": 0,
    "displayName": "Host Knight",
    "archetypeId": "ashfell_blade",
    "appearance": { },
    "inventory": { }
  },
  "guestProfile": {
    "playerSlot": 1,
    "displayName": "Guest Ranger",
    "archetypeId": "outrider",
    "appearance": { },
    "inventory": { }
  },
  "sharedCampaign": { }
}
```

## `hostProfile` / `guestProfile`

Per-human state. Same schema for both; `playerSlot` is 0 (host) or 1 (guest).

| Field | Type | Notes |
| --- | --- | --- |
| `playerSlot` | int | 0 or 1 |
| `displayName` | string | Player-chosen name |
| `archetypeId` | string | World Forge archetype id ([`world-forge-archetypes.md`](world-forge-archetypes.md)) |
| `appearance` | object | TBD — body/face/voice refs; extensible |
| `inventory` | object | Per [DEC-0050](../decisions/index.md#dec-0050-inventory-ux-item-kinds-and-positive-soft-affinity): `bag`, `hotbar`, `equipped`, optional `questInventory` / camp-stash refs; stack `count` + `itemId`. **Currencies** are non-slot counters (not bag entries). Co-op **shared gold** stays in `sharedCampaign.economy` unless a later decision splits purses |
| `stats` | object | HP, stamina, etc. when combat persistence lands |
| `inputDeviceHint` | string | Optional last-used device id for local multi-device tests |

**Solo saves:** only `hostProfile` is populated.

## `sharedCampaign`

Single source of truth for campaign progression.

### `quests`

Mirrors `QuestRuntime` session state ([`quest_runtime.h`](../../include/engine/quest/quest_runtime.h)):

```json
"quests": {
  "instances": [
    {
      "questId": "sq_01_cart_again",
      "status": "active",
      "completedObjectiveIds": ["find_pellin"]
    },
    {
      "questId": "main_act1_intro",
      "status": "completed",
      "completedObjectiveIds": ["reach_ledgeport", "meet_contact"]
    }
  ],
  "outcomeFlags": ["sq01.arkand_pride", "act1.hub_unlocked"]
}
```

| Field | Values |
| --- | --- |
| `status` | `inactive` \| `active` \| `completed` \| `abandoned` |

`outcomeFlags` stores fork/story flags not encoded in quest objective ids. Owned at runtime by `FlagRuntime` (TICKET-0225 / DEC-0046); filled via dialogue `setFlags`, `quest_resolve_fork` / `flag_set`, and round-tripped by `RpgSaveDocument::capture_from` / `hydrate_into`.

### `standing`

Mirrors `StandingRuntime` ([`standing_runtime.h`](../../include/engine/standing/standing_runtime.h)):

```json
"standing": {
  "scores": [
    { "factionId": "cristallo", "score": 12.5 },
    { "factionId": "arrotrebae", "score": -3.0 }
  ],
  "lockInFactionId": ""
}
```

Only factions with `tracksPlayer` in World Forge need entries; missing faction → implicit 0 on load.

### `morality`

Shared axis (future `MoralityRuntime`):

```json
"morality": {
  "score": 0.0,
  "bandId": "neutral",
  "unlockedArchetypeIds": []
}
```

Numeric thresholds remain story-owned; engine stores values only.

### `flags`

General story/world boolean and string flags:

```json
"flags": {
  "bools": { "act0.landfall_complete": true },
  "strings": { "allegiance.pending_choice": "" }
}
```

### `discovery`

Fog-of-war and fast-travel anchors ([DEC-0032](../decisions/index.md#dec-0032-open-world-travel-discovery-map-and-dual-soft-gates)):

```json
"discovery": {
  "revealedRegionIds": ["ohund_evergreens", "ledgeport"],
  "discoveredPostIds": ["post_ledgeport_carriage"],
  "mapFog": { }
}
```

`mapFog` encoding (bitmask vs run-length) is implementation-defined in TICKET-0114.

### `party`

```json
"party": {
  "activeCompanionIds": ["companion_arkand", "companion_pellin"],
  "maxCompanions": 3
}
```

`maxCompanions` is **derived from `sessionMode` on load** (solo 3, co-op 2) — may be stored for forward compatibility but must be validated against mode.

### `camp`

Camp instance persistence ([DEC-0033](../decisions/index.md#dec-0033-anywhere-player-camp-as-editable-instance-dao-style)):

```json
"camp": {
  "unlocked": true,
  "layout": { },
  "lastPitchAnchor": {
    "sceneId": "tessera_overland",
    "position": [120.5, 14.2, -880.0]
  }
}
```

### `economy`

Shared purse (recommended default for co-op):

```json
"economy": {
  "gold": 240
}
```

Per-player gold purses are rejected for v1 co-op unless a future decision splits them. Other currency counters (if any) follow the same shared-vs-per-player rule when introduced; they remain **non-slot** amounts, not bag entries ([DEC-0050](../decisions/index.md#dec-0050-inventory-ux-item-kinds-and-positive-soft-affinity)).

### Inventory object shape (draft)

```json
"inventory": {
  "bagCapacity": 20,
  "bag": [{ "itemId": "field_bandage", "count": 3 }],
  "hotbar": [{ "slot": 0, "itemId": "ashfell_arming_sword", "count": 1 }],
  "equipped": {
    "head": null,
    "chest": null,
    "legs": null,
    "trinket0": "vein_iron_pendant",
    "trinket1": null,
    "trinket2": null,
    "trinket3": null
  },
  "questInventory": [{ "itemId": "grenges_ledger", "count": 1 }],
  "ammo": [{ "itemId": "crude_arrow", "count": 40 }]
}
```

Camp storage is per-player and persists with the save (exact path: profile vs `sharedCampaign.camp` stash map keyed by player slot — implement under TICKET-0111 / camp tickets; must remain **per-player**). Field names may tighten when TICKET-0111 lands; semantics above are locked.

### `instances`

Optional rare instance return anchors ([DEC-0021](../decisions/index.md#dec-0021-soft-gates-with-rare-optional-instances)):

```json
"instances": {
  "activeInstanceId": "",
  "returnAnchor": null
}
```

## `worldAnchor`

Last overland pose for save/load spawn (both players spawn near anchor with slot offsets in co-op):

| Field | Notes |
| --- | --- |
| `sceneId` | Open world scene id |
| `regionId` | World Forge region for validation |
| `position` | `[x, y, z]` meters |
| `yawDegrees` | Host facing; guest offset placement beside host |

## Validation

Error prefix: `RPG-SAVE-*`

| Rule | Code (suggested) |
| --- | --- |
| `schemaVersion` supported | `RPG-SAVE-UNSUPPORTED-VERSION` |
| `sessionMode` enum | `RPG-SAVE-INVALID-MODE` |
| Co-op without `guestProfile` | `RPG-SAVE-COOP-MISSING-GUEST` |
| Solo with non-null `guestProfile` | `RPG-SAVE-SOLO-GUEST-PRESENT` |
| `hostProfile.playerSlot != 0` | `RPG-SAVE-INVALID-HOST-SLOT` |
| `guestProfile.playerSlot != 1` | `RPG-SAVE-INVALID-GUEST-SLOT` |
| Unknown quest/faction ids | warn in validate; fail on strict load optional |
| `activeCompanionIds.length > maxCompanionsForMode` | `RPG-SAVE-PARTY-OVER-CAP` |

## Migrations

- Bump `schemaVersion` for breaking changes.
- TICKET-0114 ships `migrate_vN_to_vN+1` helpers with tests for each bump.
- v0 → v1: no legacy player saves exist; greenfield.

## Runtime load sequence

```text
Parse save → validate mode + profiles
  → hydrate QuestRuntime / StandingRuntime / MoralityRuntime from sharedCampaign
  → hydrate hostProfile (+ guestProfile) into player entities
  → apply worldAnchor spawn
  → solo: playing
  → coop: lobby if guest absent; playing if guest connected
```

## Related tickets

| Ticket | Role |
| --- | --- |
| TICKET-0114 | Format + migrations + atomic save/load |
| TICKET-0180 | QuestRuntime persist mapping |
| TICKET-0181 | StandingRuntime persist mapping |
| Future | MoralityRuntime, PartyRuntime, lobby/net (see [`co-op-sessions.md`](../features/co-op-sessions.md)) |
