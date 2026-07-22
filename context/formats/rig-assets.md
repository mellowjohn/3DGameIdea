# Rig Assets (IK hooks + retarget metadata)

Status: active (TICKET-0106 / [DEC-0041](../decisions/index.md#dec-0041-rig-metadata-before-ik-solver)).

Authorable `*.rig.json` describes **IK effector hooks** and a **bone-role retarget map** for a skeleton. This slice is schema + validation only — no IK solver and no GPU skinning.

## File

- Suffix: `.rig.json`
- Schema version: `1`
- Sample: `samples/open-world-rpg/assets/characters/player.rig.json`

## Fields

| Field | Required | Meaning |
| --- | --- | --- |
| `schemaVersion` | yes | Must be `1` |
| `id` | yes | Stable slug (usually filename stem) |
| `displayName` | no | Human label |
| `mesh` | no | Optional project-relative glTF this rig describes |
| `ikHooks[]` | no | Effector hooks for a future IK solver |
| `boneRoles[]` | no | Humanoid (or custom) role → joint name |

### `ikHooks[]`

| Field | Required | Meaning |
| --- | --- | --- |
| `id` | yes | Unique hook id (`left_hand`, `right_foot`, …) |
| `tipJoint` | yes | Tip joint display name (matches `ImportedSkin::joint_names`) |
| `rootJoint` | no | Optional chain root |
| `poleJoint` | no | Optional pole / hint joint |
| `chainLength` | no | `0` = unspecified; otherwise positive count from tip toward root |

### `boneRoles[]`

| Field | Required | Meaning |
| --- | --- | --- |
| `role` | yes | Unique role id (`hips`, `left_hand`, `head`, …) |
| `jointName` | yes | Skeleton joint display name |

Recommended humanoid roles (not enforced): `hips`, `spine`, `chest`, `neck`, `head`, `left_upper_arm`, `left_lower_arm`, `left_hand`, `right_upper_arm`, `right_lower_arm`, `right_hand`, `left_upper_leg`, `left_lower_leg`, `left_foot`, `right_upper_leg`, `right_lower_leg`, `right_foot`.

## Character binding

Optional on `.character.json`:

```json
"rig": "assets/characters/player.rig.json"
```

See [`character-assets.md`](character-assets.md).

## Validation

| Code | Condition |
| --- | --- |
| `RIG-SCHEMA-UNSUPPORTED` | `schemaVersion` ≠ 1 |
| `RIG-ID-MISSING` | empty `id` |
| `RIG-IK-ID-MISSING` / `RIG-IK-TIP-MISSING` / `RIG-IK-CHAIN-INVALID` / `RIG-IK-ID-DUPLICATE` | bad hooks |
| `RIG-ROLE-MISSING` / `RIG-ROLE-JOINT-MISSING` / `RIG-ROLE-DUPLICATE` | bad roles |
| `RIG-JOINT-UNKNOWN` | `validate_against_joint_names` when a referenced joint is absent from the skin |

Joint-name checks are **optional** (empty joint list skips). Use them when a skinned mesh is available.

## C++ API

- `RigAsset::load` / `from_json` / `to_json` / `save` / `validate`
- `RigAsset::validate_against_joint_names(joint_names)`

## Out of scope (this ticket)

- Runtime IK solve (FABRIK / Two-bone / etc.) — owner path is metadata now, full IK later
- GPU skinning / pose playback
- Editor Animation tools panel (TICKET-0135)
- Auto-generating roles from glTF node names

## Related

- [`mesh-assets.md`](mesh-assets.md) skeletal subset
- [`animation-clip-assets.md`](animation-clip-assets.md)
- [`../features/animator.md`](../features/animator.md)
