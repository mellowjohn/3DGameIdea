# Script Assets

## bindings.script.json

Versioned binding file, default path: `assets/scripts/bindings.script.json`.

```json
{
  "schemaVersion": 1,
  "interactions": [
    { "id": "use_campfire", "handler": "on_use_campfire", "script": "assets/scripts/campfire_interaction.lua" }
  ],
  "combatHits": [],
  "combatHurts": [
    { "id": "body", "handler": "on_body_hit", "script": "assets/scripts/combat_hurt.lua" }
  ]
}
```

- `id` matches prefab `interaction`, `combatHit`, or `combatHurt` collision fields
- `handler` is the global Lua function name
- `script` is a project-relative Lua asset path

Editor **Open Script** (TICKET-0149) resolves a component's `kind` + `bindingId` through this file to the `script` path and opens it with the OS default association.

## Lua files

Store gameplay handlers under `assets/scripts/`. Each referenced script must compile in the sandbox and define the bound handler as a global function.

Invalid scripts are rejected before hot reload is applied.
