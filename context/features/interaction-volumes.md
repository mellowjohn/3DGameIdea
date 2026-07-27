# Interaction Volumes

Prefab-authored trigger volumes that emit gameplay-facing enter/exit events.

## Authoring

Add an `interaction` string to a prefab `collision` entry (see `context/formats/prefab-assets.md`). When present, the volume is forced to `trigger` semantics and registered as an interaction sensor.

```json
{
  "shape": "sphere",
  "interaction": "use_campfire",
  "transform": { "position": [0.0, 0.55, 0.0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1] },
  "radius": 0.85
}
```

`assets/prefabs/campfire.prefab.json` in the sample project uses `use_campfire`.

## Runtime API

- `InteractionVolumeRegistry` maps `CollisionBody` tokens to `{placement_entity_id, volume_index, interaction_id}`.
- `PlacementCollisionTracker::interaction_registry()` rebuilds bindings when placed prefab collision syncs.
- `InteractionOverlapTracker::update(interactor_id, center, radius, world, registry)` compares trigger overlaps frame-to-frame and returns `InteractionEvent` enter/exit records without requiring a physics step.

## Debug integration

- **Debug world**: a `use_campfire` probe sphere spawns near the origin; the character overlap tracker fires enter/exit while walking into it.
- **Editor**: Diagnostics **Show event zones** draws all authored interaction volumes (violet + labels) without enabling full collision debug. **Show collision debug** still draws interaction volumes in gold among all physics bodies. Physics contact events against registered interaction triggers append to **Recent interactions** in Diagnostics when placement collision is active.

## Interaction prompt UX

Volumes that should be discoverable use enter/exit to drive a world-space billboard (`WorldUiBillboardRuntime`) via blackboard keys `interact.prompt` / `interact.id` / `interact.label` / `interact.x|y|z`. Press **E** dispatches `use` to the current `interact.id`.

| Interaction id | Prompt label | Activate on |
| --- | --- | --- |
| `talk_sandbox` / `talk_act0` | Press E to talk | `use` |
| `use_campfire` | Press E to rest | `use` (heal + sound) |
| `event_sandbox` | Press E to investigate | `use` (timeline start) |

## Limitations

- Capsule-accurate interaction remains future work; overlap queries use a spherical probe.
- Entity-attached billboards (follow transform each frame) are follow-on; prompts currently use contact-point world positions.

## Dialogue hooks

Interaction scripts can start World Forge trees through the explicit dialogue API (DEC-0028 — no auto quest advance):

1. Lookup: `engine.quest_dialogue_hook(questId, "start"|"current"|...)`
2. Start: `engine.dialogue_start(treeId)` then `dialogue_present` / `dialogue_choose`
3. Campaign sample: `talk_act0` → `talk_act0_interaction.lua` (Act 0 story trees)
4. Sandbox sample: `talk_sandbox` → `dlg_sandbox_sample` (short test tree; Press E after enter)
5. Sandbox event zone: `event_sandbox` → `evt_sandbox_zone_pan` (multi-subject cinematic look_at NPC → campfire → oak, then `dlg_sandbox_event_zone`; separate from practice-keeper talk)
6. MCP: `engine_dialogue_call` / `engine_lua_call` kind=interaction
7. Thin presentation: `dialogue` UI canvas (wrap + scroll + typewriter); polished multi-choice UI remains `ui_dialogue_presentation`
