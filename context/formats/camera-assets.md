# Camera Assets

Camera assets use the `.camera.json` suffix and schema version 1. They define third-person RPG orbit camera behavior for editor test sessions (WoW / Dragon Age–style behind-and-above framing).

## Fields

- `pivotHeight`: shoulder/chest orbit pivot height above character feet (meters).
- `minDistance`, `maxDistance`, `defaultDistance`: orbit radius limits and default.
- `shoulderOffset`: lateral eye offset in meters (positive = over the right shoulder); look-at stays on the character.
- `defaultPitch`, `minPitch`, `maxPitch`: look-down pitch in radians (session start + clamp).
- `collisionProbeRadius`, `collisionPadding`: camera collision shortening probe.
- `lookSensitivity`: mouse-look multiplier.
- `zoomSensitivity`: meters of orbit distance change per mouse-wheel notch.
- `verticalFovRadians`, `nearPlane`, `farPlane`: perspective projection parameters.

## Project binding

Referenced from `play.session.json` alongside the character asset.

## Editor

Editable from the Inspector: click a `.camera.json` in the Asset Browser (or open Play Session bindings with nothing selected). Use **Save Camera Asset** to persist. During a test session, distance limits, shoulder, pitch, FOV, and zoom sensitivity all apply live; Save writes them to disk.
