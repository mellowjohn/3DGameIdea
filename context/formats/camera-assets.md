# Camera Assets

Camera assets use the `.camera.json` suffix and schema version 1. They define third-person orbit camera behavior for editor test sessions.

## Fields

- `pivotHeight`: shoulder/look pivot height above character feet in meters.
- `minDistance`, `maxDistance`, `defaultDistance`: orbit radius limits and default.
- `collisionProbeRadius`, `collisionPadding`: camera collision shortening probe.
- `lookSensitivity`: mouse-look multiplier.
- `verticalFovRadians`, `nearPlane`, `farPlane`: perspective projection parameters.

## Project binding

Referenced from `play.session.json` alongside the character asset.

## Editor

Editable from the Inspector play-session panel or via Asset Browser **Inspect** on a `.camera.json` file. Use **Save Camera Asset** to persist changes. Values apply on the next test session start.
